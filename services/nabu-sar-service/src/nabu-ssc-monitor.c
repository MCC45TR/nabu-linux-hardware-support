#include "nabu-ssc-monitor.h"

#include <libssc/libssc-sensor.h>

#include "ssc-monitor-state.h"

#define OBJECT_PATH "/org/senemos/Nabu/Sensors"
#define INTERFACE_NAME "org.senemos.Nabu.Sensors1"
#define DISCOVERY_TIMEOUT_SECONDS 15
#define ENABLE_TIMEOUT_SECONDS 8
#define REPORT_FRESH_USEC (60 * G_USEC_PER_SEC)

typedef struct _Algorithm Algorithm;

struct _Algorithm {
	struct _NabuSscMonitor *monitor;
	const gchar *data_type;
	const gchar *display_name;
	SSCSensor *sensor;
	GObject *client;
	GCancellable *cancellable;
	gulong report_id;
	guint timeout_id;
	guint64 uid_high;
	guint64 uid_low;
	guint stream_type;
	gchar *name;
	gchar *vendor;
	gboolean discovered;
	gboolean available;
	gboolean complete;
	gboolean fresh_at_last_emit;
	NabuSscMonitorState state;
};

struct _NabuSscMonitor {
	GDBusConnection *bus;
	GDBusNodeInfo *introspection;
	guint registration_id;
	guint emit_timer_id;
	guint advance_idle_id;
	guint next_index;
	gboolean started;
	gboolean ready;
	gboolean dirty;
	Algorithm *algorithms;
	guint algorithm_count;
};

static const struct {
	const gchar *data_type;
	const gchar *display_name;
} algorithm_catalog[] = {
	{ "tilt_to_wake", "Tilt to wake" },
	{ "tilt", "Tilt" },
	{ "pickup", "Pickup" },
	{ "screen_down", "Screen down" },
	{ "pedometer", "Pedometer" },
	{ "basic_gestures", "Basic gestures" },
	{ "bring_to_ear", "Bring to ear" },
	{ "multishake", "Multi-shake" },
	{ "device_orient", "Device orientation" },
	{ "facing", "Facing" },
	{ "sig_motion", "Significant motion" },
	{ "motion_detect", "Motion detect" },
	{ "oem_step_detector", "OEM step detector" },
	{ "gravity", "Gravity" },
	{ "sar_algo_1", "SAR algorithm" },
	{ "psmd", "Persistent significant motion" },
	{ "3d_signature", "3D signature" },
};

static const gchar introspection_xml[] =
"<node>"
" <interface name='org.senemos.Nabu.Sensors1'>"
"  <property name='InterfaceVersion' type='u' access='read'/>"
"  <property name='Ready' type='b' access='read'/>"
"  <property name='MonitoringPolicy' type='s' access='read'/>"
"  <property name='Algorithms' type='aa{sv}' access='read'/>"
"  <property name='TotalReports' type='t' access='read'/>"
" </interface>"
"</node>";

static gboolean start_next_idle(gpointer user_data);

static gboolean
algorithm_report_fresh(const Algorithm *algorithm, gint64 now_usec)
{
	return nabu_ssc_monitor_state_report_fresh(&algorithm->state, now_usec,
		REPORT_FRESH_USEC);
}

static GVariant *
build_algorithms_variant(NabuSscMonitor *monitor)
{
	GVariantBuilder algorithms;
	gint64 now_usec = g_get_monotonic_time();

	g_variant_builder_init(&algorithms, G_VARIANT_TYPE("aa{sv}"));
	for (guint i = 0; i < monitor->algorithm_count; i++) {
		Algorithm *algorithm = &monitor->algorithms[i];
		GVariantBuilder properties;

		g_variant_builder_init(&properties, G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add(&properties, "{sv}", "DataType",
			g_variant_new_string(algorithm->data_type));
		g_variant_builder_add(&properties, "{sv}", "DisplayName",
			g_variant_new_string(algorithm->display_name));
		g_variant_builder_add(&properties, "{sv}", "Name",
			g_variant_new_string(algorithm->name ?: ""));
		g_variant_builder_add(&properties, "{sv}", "Vendor",
			g_variant_new_string(algorithm->vendor ?: ""));
		g_variant_builder_add(&properties, "{sv}", "Status",
			g_variant_new_string(nabu_ssc_monitor_state_status(&algorithm->state)));
		g_variant_builder_add(&properties, "{sv}", "StreamType",
			g_variant_new_string(nabu_ssc_stream_type_to_string(algorithm->stream_type)));
		g_variant_builder_add(&properties, "{sv}", "Discovered",
			g_variant_new_boolean(algorithm->discovered));
		g_variant_builder_add(&properties, "{sv}", "Available",
			g_variant_new_boolean(algorithm->available));
		g_variant_builder_add(&properties, "{sv}", "Monitoring",
			g_variant_new_boolean(algorithm->state.stage == NABU_SSC_STAGE_MONITORING));
		g_variant_builder_add(&properties, "{sv}", "ReportObserved",
			g_variant_new_boolean(algorithm->state.report_observed));
		g_variant_builder_add(&properties, "{sv}", "ReportFresh",
			g_variant_new_boolean(algorithm_report_fresh(algorithm, now_usec)));
		g_variant_builder_add(&properties, "{sv}", "ReportCount",
			g_variant_new_uint64(algorithm->state.report_count));
		g_variant_builder_add(&properties, "{sv}", "LastReportMessageId",
			g_variant_new_uint32(algorithm->state.last_report_message_id));
		g_variant_builder_add(&properties, "{sv}", "LastReportMonotonicUsec",
			g_variant_new_int64(algorithm->state.last_report_usec));
		g_variant_builder_add_value(&algorithms,
			g_variant_builder_end(&properties));
	}

	return g_variant_builder_end(&algorithms);
}

static guint64
total_reports(NabuSscMonitor *monitor)
{
	guint64 total = 0;

	for (guint i = 0; i < monitor->algorithm_count; i++) {
		guint64 count = monitor->algorithms[i].state.report_count;
		if (G_MAXUINT64 - total < count)
			return G_MAXUINT64;
		total += count;
	}
	return total;
}

static void
emit_properties_changed(NabuSscMonitor *monitor)
{
	GVariantBuilder changed, invalidated;

	if (!monitor->registration_id)
		return;
	g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&changed, "{sv}", "Ready",
		g_variant_new_boolean(monitor->ready));
	g_variant_builder_add(&changed, "{sv}", "Algorithms",
		build_algorithms_variant(monitor));
	g_variant_builder_add(&changed, "{sv}", "TotalReports",
		g_variant_new_uint64(total_reports(monitor)));
	g_variant_builder_init(&invalidated, G_VARIANT_TYPE("as"));
	g_dbus_connection_emit_signal(monitor->bus, NULL, OBJECT_PATH,
		"org.freedesktop.DBus.Properties", "PropertiesChanged",
		g_variant_new("(sa{sv}as)", INTERFACE_NAME, &changed, &invalidated),
		NULL);
	monitor->dirty = FALSE;
	for (guint i = 0; i < monitor->algorithm_count; i++)
		monitor->algorithms[i].fresh_at_last_emit =
			algorithm_report_fresh(&monitor->algorithms[i],
				g_get_monotonic_time());
}

static gboolean
emit_timer(gpointer user_data)
{
	NabuSscMonitor *monitor = user_data;
	gint64 now_usec = g_get_monotonic_time();

	for (guint i = 0; i < monitor->algorithm_count; i++) {
		Algorithm *algorithm = &monitor->algorithms[i];
		if (algorithm->fresh_at_last_emit !=
		    algorithm_report_fresh(algorithm, now_usec)) {
			monitor->dirty = TRUE;
			break;
		}
	}
	if (monitor->dirty)
		emit_properties_changed(monitor);
	return G_SOURCE_CONTINUE;
}

static GVariant *
get_property(GDBusConnection *connection, const gchar *sender,
		     const gchar *object_path, const gchar *interface_name,
		     const gchar *property_name, GError **error,
		     gpointer user_data)
{
	NabuSscMonitor *monitor = user_data;
	(void)connection;
	(void)sender;
	(void)object_path;
	(void)interface_name;

	if (!g_strcmp0(property_name, "InterfaceVersion"))
		return g_variant_new_uint32(1);
	if (!g_strcmp0(property_name, "Ready"))
		return g_variant_new_boolean(monitor->ready);
	if (!g_strcmp0(property_name, "MonitoringPolicy"))
		return g_variant_new_string("on-change-and-single-output-only");
	if (!g_strcmp0(property_name, "Algorithms"))
		return build_algorithms_variant(monitor);
	if (!g_strcmp0(property_name, "TotalReports"))
		return g_variant_new_uint64(total_reports(monitor));
	g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
		"unknown property %s", property_name);
	return NULL;
}

static const GDBusInterfaceVTable vtable = {
	.get_property = get_property,
};

static void
schedule_next(NabuSscMonitor *monitor)
{
	if (!monitor->advance_idle_id)
		monitor->advance_idle_id = g_idle_add(start_next_idle, monitor);
}

static void
finish_algorithm(Algorithm *algorithm)
{
	NabuSscMonitor *monitor = algorithm->monitor;

	if (algorithm->complete)
		return;
	algorithm->complete = TRUE;
	if (algorithm->timeout_id) {
		g_source_remove(algorithm->timeout_id);
		algorithm->timeout_id = 0;
	}
	monitor->dirty = TRUE;
	monitor->next_index++;
	schedule_next(monitor);
}

static gboolean
algorithm_timeout(gpointer user_data)
{
	Algorithm *algorithm = user_data;

	algorithm->timeout_id = 0;
	if (algorithm->complete)
		return G_SOURCE_REMOVE;
	if (algorithm->state.report_observed)
		algorithm->state.stage = NABU_SSC_STAGE_MONITORING;
	else
		algorithm->state.stage = NABU_SSC_STAGE_TIMEOUT;
	if (algorithm->cancellable)
		g_cancellable_cancel(algorithm->cancellable);
	finish_algorithm(algorithm);
	return G_SOURCE_REMOVE;
}

static void
report_received(gpointer client, guint32 message_id, guint64 uid_high,
		guint64 uid_low, GArray *buffer, gpointer user_data)
{
	Algorithm *algorithm = user_data;
	(void)client;
	(void)buffer;

	if (uid_high != algorithm->uid_high || uid_low != algorithm->uid_low)
		return;
	if (!nabu_ssc_monitor_state_record_report(&algorithm->state, message_id,
		    g_get_monotonic_time()))
		return;
	algorithm->monitor->dirty = TRUE;
}

static void
sensor_opened(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Algorithm *algorithm = user_data;
	g_autoptr(GError) error = NULL;
	(void)source;

	if (algorithm->complete)
		return;
	if (!ssc_sensor_open_finish(algorithm->sensor, result, &error)) {
		if (!algorithm->state.report_observed)
			algorithm->state.stage = NABU_SSC_STAGE_ENABLE_FAILED;
	} else {
		algorithm->state.stage = NABU_SSC_STAGE_MONITORING;
	}
	finish_algorithm(algorithm);
}

static gboolean
sensor_property_abi_is_compatible(SSCSensor *sensor)
{
	GObjectClass *object_class = G_OBJECT_GET_CLASS(sensor);
	GParamSpec *available = g_object_class_find_property(object_class,
		SSC_SENSOR_AVAILABLE);
	GParamSpec *stream_type = g_object_class_find_property(object_class,
		SSC_SENSOR_STREAM_TYPE);

	return available && stream_type &&
		G_PARAM_SPEC_VALUE_TYPE(available) == G_TYPE_BOOLEAN &&
		G_PARAM_SPEC_VALUE_TYPE(stream_type) == G_TYPE_UINT;
}

static void
sensor_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Algorithm *algorithm = user_data;
	g_autoptr(GError) error = NULL;
	SSCSensor *sensor;
	(void)source;

	sensor = ssc_sensor_new_finish(result, &error);
	if (algorithm->complete) {
		g_clear_object(&sensor);
		return;
	}
	if (algorithm->timeout_id) {
		g_source_remove(algorithm->timeout_id);
		algorithm->timeout_id = 0;
	}
	if (!sensor) {
		algorithm->state.stage = NABU_SSC_STAGE_UNAVAILABLE;
		finish_algorithm(algorithm);
		return;
	}

	algorithm->sensor = sensor;
	algorithm->discovered = TRUE;
	g_object_get(algorithm->sensor,
		SSC_SENSOR_UID_HIGH, &algorithm->uid_high,
		SSC_SENSOR_UID_LOW, &algorithm->uid_low,
		SSC_SENSOR_NAME, &algorithm->name,
		SSC_SENSOR_VENDOR, &algorithm->vendor,
		SSC_SENSOR_CLIENT, &algorithm->client,
		NULL);
	algorithm->state.stage = NABU_SSC_STAGE_DISCOVERED;
	if (!sensor_property_abi_is_compatible(algorithm->sensor)) {
		/* Older libssc declares scalar properties as strings.  Reading those
		 * properties would be memory-unsafe; remain discovery-only instead. */
		algorithm->state.stage = NABU_SSC_STAGE_INCOMPATIBLE_ABI;
		finish_algorithm(algorithm);
		return;
	}
	g_object_get(algorithm->sensor,
		SSC_SENSOR_AVAILABLE, &algorithm->available,
		SSC_SENSOR_STREAM_TYPE, &algorithm->stream_type,
		NULL);
	if (!algorithm->available) {
		finish_algorithm(algorithm);
		return;
	}
	if (!nabu_ssc_stream_type_is_safe_to_monitor(algorithm->stream_type)) {
		algorithm->state.stage = NABU_SSC_STAGE_UNSUPPORTED_STREAM;
		finish_algorithm(algorithm);
		return;
	}

	algorithm->report_id = g_signal_connect(algorithm->client, "report",
		G_CALLBACK(report_received), algorithm);
	algorithm->state.stage = NABU_SSC_STAGE_ENABLING;
	algorithm->timeout_id = g_timeout_add_seconds(ENABLE_TIMEOUT_SECONDS,
		algorithm_timeout, algorithm);
	ssc_sensor_open(algorithm->sensor, algorithm->cancellable,
		sensor_opened, algorithm);
}

static gboolean
start_next_idle(gpointer user_data)
{
	NabuSscMonitor *monitor = user_data;
	Algorithm *algorithm;
	monitor->advance_idle_id = 0;

	if (monitor->next_index >= monitor->algorithm_count) {
		monitor->ready = TRUE;
		monitor->dirty = TRUE;
		return G_SOURCE_REMOVE;
	}

	algorithm = &monitor->algorithms[monitor->next_index];
	algorithm->state.stage = NABU_SSC_STAGE_DISCOVERING;
	algorithm->cancellable = g_cancellable_new();
	algorithm->timeout_id = g_timeout_add_seconds(DISCOVERY_TIMEOUT_SECONDS,
		algorithm_timeout, algorithm);
	monitor->dirty = TRUE;
	ssc_sensor_new((gchar *)algorithm->data_type, algorithm->cancellable,
		sensor_ready, algorithm);
	return G_SOURCE_REMOVE;
}

NabuSscMonitor *
nabu_ssc_monitor_new(GDBusConnection *bus, GError **error)
{
	NabuSscMonitor *monitor;

	g_return_val_if_fail(G_IS_DBUS_CONNECTION(bus), NULL);
	monitor = g_new0(NabuSscMonitor, 1);
	monitor->bus = g_object_ref(bus);
	monitor->algorithm_count = G_N_ELEMENTS(algorithm_catalog);
	monitor->algorithms = g_new0(Algorithm, monitor->algorithm_count);
	for (guint i = 0; i < monitor->algorithm_count; i++) {
		monitor->algorithms[i].monitor = monitor;
		monitor->algorithms[i].data_type = algorithm_catalog[i].data_type;
		monitor->algorithms[i].display_name = algorithm_catalog[i].display_name;
		monitor->algorithms[i].stream_type = G_MAXUINT;
		monitor->algorithms[i].state.stage = NABU_SSC_STAGE_QUEUED;
	}
	monitor->introspection = g_dbus_node_info_new_for_xml(introspection_xml,
		error);
	if (!monitor->introspection)
		goto fail;
	monitor->registration_id = g_dbus_connection_register_object(bus,
		OBJECT_PATH, monitor->introspection->interfaces[0], &vtable,
		monitor, NULL, error);
	if (!monitor->registration_id)
		goto fail;
	return monitor;

fail:
	nabu_ssc_monitor_free(monitor);
	return NULL;
}

void
nabu_ssc_monitor_start(NabuSscMonitor *monitor)
{
	g_return_if_fail(monitor != NULL);
	if (monitor->started)
		return;
	monitor->started = TRUE;
	monitor->dirty = TRUE;
	monitor->emit_timer_id = g_timeout_add_seconds(1, emit_timer, monitor);
	schedule_next(monitor);
}

void
nabu_ssc_monitor_free(NabuSscMonitor *monitor)
{
	if (!monitor)
		return;
	if (monitor->emit_timer_id)
		g_source_remove(monitor->emit_timer_id);
	if (monitor->advance_idle_id)
		g_source_remove(monitor->advance_idle_id);
	for (guint i = 0; i < monitor->algorithm_count; i++) {
		Algorithm *algorithm = &monitor->algorithms[i];
		if (algorithm->timeout_id)
			g_source_remove(algorithm->timeout_id);
		if (algorithm->cancellable)
			g_cancellable_cancel(algorithm->cancellable);
		if (algorithm->report_id && algorithm->client)
			g_signal_handler_disconnect(algorithm->client,
				algorithm->report_id);
		g_clear_object(&algorithm->client);
		g_clear_object(&algorithm->sensor);
		g_clear_object(&algorithm->cancellable);
		g_clear_pointer(&algorithm->name, g_free);
		g_clear_pointer(&algorithm->vendor, g_free);
	}
	if (monitor->registration_id && monitor->bus)
		g_dbus_connection_unregister_object(monitor->bus,
			monitor->registration_id);
	g_clear_pointer(&monitor->introspection, g_dbus_node_info_unref);
	g_clear_object(&monitor->bus);
	g_free(monitor->algorithms);
	g_free(monitor);
}
