#include <gio/gio.h>
#include <glib-unix.h>
#include <libssc/libssc-sensor.h>
#include <unistd.h>

#include "sar-parser.h"
#include "sar-state.h"

#define SSC_MSG_REPORT_MEASUREMENT 1025
#define BUS_NAME "org.senemos.Nabu.Sar"
#define OBJECT_PATH "/org/senemos/Nabu/Sar"
#define INTERFACE_NAME "org.senemos.Nabu.Sar1"
#define CONFIG_PATH "/etc/nabu-sar.conf"
#define HOLD_AWAKE_STATE_PATH "/var/lib/nabu-sar/hold-awake-enabled"
#define SAMPLE_STALE_USEC (3 * G_USEC_PER_SEC)
#define INHIBITOR_RETRY_USEC (5 * G_USEC_PER_SEC)

typedef struct {
	GMainLoop *loop;
	GDBusConnection *bus;
	GDBusNodeInfo *introspection;
	guint registration_id;
	guint owner_id;
	SSCSensor *sensor;
	GObject *client;
	gulong report_id;
	guint stale_timer_id;
	guint64 uid_high;
	guint64 uid_low;
	gchar *name;
	gchar *vendor;
	gboolean available;
	gboolean sample_fresh;
	gboolean hold_awake_enabled;
	gint inhibitor_fd;
	gint64 last_sample_usec;
	gint64 last_inhibitor_attempt_usec;
	NabuSarSample sample;
	NabuSarClassifier classifier;
} Service;

static const gchar introspection_xml[] =
"<node>"
" <interface name='org.senemos.Nabu.Sar1'>"
"  <method name='SetHoldAwakeEnabled'>"
"   <arg name='enabled' type='b' direction='in'/>"
"  </method>"
"  <property name='Available' type='b' access='read'/>"
"  <property name='Name' type='s' access='read'/>"
"  <property name='Vendor' type='s' access='read'/>"
"  <property name='Values' type='ad' access='read'/>"
"  <property name='Deltas' type='ad' access='read'/>"
"  <property name='RawValues' type='ad' access='read'/>"
"  <property name='Baselines' type='ad' access='read'/>"
"  <property name='Accuracy' type='u' access='read'/>"
"  <property name='GripState' type='s' access='read'/>"
"  <property name='MappingEnabled' type='b' access='read'/>"
"  <property name='SampleFresh' type='b' access='read'/>"
"  <property name='HoldAwakeEnabled' type='b' access='read'/>"
"  <property name='SleepInhibited' type='b' access='read'/>"
" </interface>"
"</node>";

static GVariant *
double_array(const gfloat *values, guint count)
{
	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE("ad"));
	for (guint i = 0; i < count; i++)
		g_variant_builder_add(&builder, "d", (gdouble)values[i]);
	return g_variant_builder_end(&builder);
}

static void
emit_properties_changed(Service *service)
{
	GVariantBuilder changed, invalidated;
	g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&changed, "{sv}", "Available", g_variant_new_boolean(service->available));
	g_variant_builder_add(&changed, "{sv}", "Values", double_array(service->sample.values, NABU_SAR_VALUE_COUNT));
	g_variant_builder_add(&changed, "{sv}", "Deltas", double_array(service->sample.delta, NABU_SAR_CHANNEL_COUNT));
	g_variant_builder_add(&changed, "{sv}", "RawValues", double_array(service->sample.raw, NABU_SAR_CHANNEL_COUNT));
	g_variant_builder_add(&changed, "{sv}", "Baselines", double_array(service->sample.baseline, NABU_SAR_CHANNEL_COUNT));
	g_variant_builder_add(&changed, "{sv}", "Accuracy", g_variant_new_uint32(service->sample.accuracy));
	g_variant_builder_add(&changed, "{sv}", "GripState", g_variant_new_string(nabu_sar_state_to_string(service->classifier.state)));
	g_variant_builder_add(&changed, "{sv}", "MappingEnabled", g_variant_new_boolean(service->classifier.enabled));
	g_variant_builder_add(&changed, "{sv}", "SampleFresh", g_variant_new_boolean(service->sample_fresh));
	g_variant_builder_add(&changed, "{sv}", "HoldAwakeEnabled", g_variant_new_boolean(service->hold_awake_enabled));
	g_variant_builder_add(&changed, "{sv}", "SleepInhibited", g_variant_new_boolean(service->inhibitor_fd >= 0));
	g_variant_builder_init(&invalidated, G_VARIANT_TYPE("as"));
	g_dbus_connection_emit_signal(service->bus, NULL, OBJECT_PATH,
		"org.freedesktop.DBus.Properties", "PropertiesChanged",
		g_variant_new("(sa{sv}as)", INTERFACE_NAME, &changed, &invalidated), NULL);
}

static void
release_inhibitor(Service *service)
{
	if (service->inhibitor_fd < 0)
		return;
	close(service->inhibitor_fd);
	service->inhibitor_fd = -1;
	g_message("released logind sleep inhibitor");
}

static void
update_inhibitor(Service *service)
{
	gboolean required = nabu_sar_should_inhibit(service->hold_awake_enabled,
		service->classifier.enabled, service->sample_fresh,
		service->classifier.state);

	if (!required) {
		release_inhibitor(service);
		return;
	}
	if (service->inhibitor_fd >= 0)
		return;

	gint64 now = g_get_monotonic_time();
	if (now - service->last_inhibitor_attempt_usec < INHIBITOR_RETRY_USEC)
		return;
	service->last_inhibitor_attempt_usec = now;

	g_autoptr(GError) error = NULL;
	g_autoptr(GUnixFDList) reply_fds = NULL;
	g_autoptr(GVariant) reply = g_dbus_connection_call_with_unix_fd_list_sync(
		service->bus,
		"org.freedesktop.login1",
		"/org/freedesktop/login1",
		"org.freedesktop.login1.Manager",
		"Inhibit",
		g_variant_new("(ssss)", "idle:sleep", "nabu-sar-service",
			"Tablet is being held", "block"),
		G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, 5000,
		NULL, &reply_fds, NULL, &error);
	if (!reply) {
		g_warning("cannot acquire logind sleep inhibitor: %s", error->message);
		return;
	}

	gint handle = -1;
	g_variant_get(reply, "(h)", &handle);
	service->inhibitor_fd = g_unix_fd_list_get(reply_fds, handle, &error);
	if (service->inhibitor_fd < 0) {
		g_warning("cannot retain logind sleep inhibitor: %s", error->message);
		return;
	}
	g_message("acquired logind sleep inhibitor while tablet is held");
}

static gboolean
persist_hold_awake_enabled(gboolean enabled, GError **error)
{
	return g_file_set_contents(HOLD_AWAKE_STATE_PATH, enabled ? "1\n" : "0\n", -1, error);
}

static void
load_hold_awake_enabled(Service *service)
{
	g_autofree gchar *contents = NULL;
	service->hold_awake_enabled = FALSE;
	if (g_file_get_contents(HOLD_AWAKE_STATE_PATH, &contents, NULL, NULL))
		service->hold_awake_enabled = g_str_has_prefix(contents, "1");
}

static void
load_configuration(Service *service)
{
	g_autoptr(GKeyFile) key_file = g_key_file_new();
	g_autoptr(GError) error = NULL;

	service->classifier.enabled = FALSE;
	service->classifier.channel_mask = 0x5;
	service->classifier.held_threshold = 500.0f;
	service->classifier.released_threshold = 250.0f;
	service->classifier.debounce_samples = 3;
	service->classifier.candidate_count = 0;
	service->classifier.state = NABU_SAR_STATE_UNKNOWN;

	if (!g_key_file_load_from_file(key_file, CONFIG_PATH, G_KEY_FILE_NONE, &error)) {
		g_message("mapping disabled; configuration unavailable: %s", error->message);
		return;
	}
	service->classifier.enabled = g_key_file_get_boolean(key_file, "Mapping", "Enabled", NULL);
	service->classifier.channel_mask = g_key_file_get_uint64(key_file, "Mapping", "ChannelMask", NULL);
	service->classifier.held_threshold = g_key_file_get_double(key_file, "Mapping", "HeldThreshold", NULL);
	service->classifier.released_threshold = g_key_file_get_double(key_file, "Mapping", "ReleasedThreshold", NULL);
	service->classifier.debounce_samples = g_key_file_get_uint64(key_file, "Mapping", "DebounceSamples", NULL);
	if (service->classifier.held_threshold <= service->classifier.released_threshold ||
	    !service->classifier.channel_mask) {
		g_warning("invalid SAR mapping; disabling classifier");
		service->classifier.enabled = FALSE;
	}
}

static GVariant *
get_property(GDBusConnection *connection, const gchar *sender,
		const gchar *object_path, const gchar *interface_name,
		const gchar *property_name, GError **error, gpointer user_data)
{
	Service *s = user_data;
	if (!g_strcmp0(property_name, "Available")) return g_variant_new_boolean(s->available);
	if (!g_strcmp0(property_name, "Name")) return g_variant_new_string(s->name ?: "");
	if (!g_strcmp0(property_name, "Vendor")) return g_variant_new_string(s->vendor ?: "");
	if (!g_strcmp0(property_name, "Values")) return double_array(s->sample.values, NABU_SAR_VALUE_COUNT);
	if (!g_strcmp0(property_name, "Deltas")) return double_array(s->sample.delta, NABU_SAR_CHANNEL_COUNT);
	if (!g_strcmp0(property_name, "RawValues")) return double_array(s->sample.raw, NABU_SAR_CHANNEL_COUNT);
	if (!g_strcmp0(property_name, "Baselines")) return double_array(s->sample.baseline, NABU_SAR_CHANNEL_COUNT);
	if (!g_strcmp0(property_name, "Accuracy")) return g_variant_new_uint32(s->sample.accuracy);
	if (!g_strcmp0(property_name, "GripState")) return g_variant_new_string(nabu_sar_state_to_string(s->classifier.state));
	if (!g_strcmp0(property_name, "MappingEnabled")) return g_variant_new_boolean(s->classifier.enabled);
	if (!g_strcmp0(property_name, "SampleFresh")) return g_variant_new_boolean(s->sample_fresh);
	if (!g_strcmp0(property_name, "HoldAwakeEnabled")) return g_variant_new_boolean(s->hold_awake_enabled);
	if (!g_strcmp0(property_name, "SleepInhibited")) return g_variant_new_boolean(s->inhibitor_fd >= 0);
	g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "unknown property %s", property_name);
	return NULL;
}

static void
method_call(GDBusConnection *connection, const gchar *sender,
		const gchar *object_path, const gchar *interface_name,
		const gchar *method_name, GVariant *parameters,
		GDBusMethodInvocation *invocation, gpointer user_data)
{
	Service *s = user_data;
	if (g_strcmp0(method_name, "SetHoldAwakeEnabled")) {
		g_dbus_method_invocation_return_error(invocation, G_IO_ERROR,
			G_IO_ERROR_NOT_SUPPORTED, "unknown method %s", method_name);
		return;
	}

	gboolean enabled;
	g_variant_get(parameters, "(b)", &enabled);
	g_autoptr(GError) error = NULL;
	if (!persist_hold_awake_enabled(enabled, &error)) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		return;
	}
	s->hold_awake_enabled = enabled;
	update_inhibitor(s);
	emit_properties_changed(s);
	g_dbus_method_invocation_return_value(invocation, NULL);
}

static const GDBusInterfaceVTable vtable = {
	.method_call = method_call,
	.get_property = get_property,
};

static void
report_received(gpointer client, guint32 msg_id, guint64 uid_high,
		guint64 uid_low, GArray *buffer, gpointer user_data)
{
	Service *s = user_data;
	g_autoptr(GError) error = NULL;
	if (msg_id != SSC_MSG_REPORT_MEASUREMENT || uid_high != s->uid_high || uid_low != s->uid_low)
		return;
	if (!nabu_sar_parse_report((const guint8 *)buffer->data, buffer->len, &s->sample, &error)) {
		g_warning("cannot decode ADUX1050 report: %s", error->message);
		return;
	}
	s->last_sample_usec = g_get_monotonic_time();
	s->sample_fresh = TRUE;
	nabu_sar_classifier_update(&s->classifier, &s->sample);
	update_inhibitor(s);
	emit_properties_changed(s);
}

static gboolean
check_sample_freshness(gpointer user_data)
{
	Service *s = user_data;
	if (!s->sample_fresh ||
	    g_get_monotonic_time() - s->last_sample_usec <= SAMPLE_STALE_USEC)
		return G_SOURCE_CONTINUE;
	s->sample_fresh = FALSE;
	s->classifier.state = NABU_SAR_STATE_UNKNOWN;
	s->classifier.candidate_count = 0;
	update_inhibitor(s);
	emit_properties_changed(s);
	g_warning("SAR sample stream became stale; released any sleep inhibitor");
	return G_SOURCE_CONTINUE;
}

static void
sensor_opened(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Service *s = user_data;
	g_autoptr(GError) error = NULL;
	if (!ssc_sensor_open_finish(s->sensor, result, &error)) {
		g_warning("cannot enable ADUX1050: %s", error->message);
		return;
	}
	s->available = TRUE;
	emit_properties_changed(s);
}

static void
sensor_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Service *s = user_data;
	g_autoptr(GError) error = NULL;
	s->sensor = ssc_sensor_new_finish(result, &error);
	if (!s->sensor) {
		g_warning("SAR ADUX1050 unavailable: %s", error->message);
		return;
	}
	g_object_get(s->sensor, SSC_SENSOR_CLIENT, &s->client,
		SSC_SENSOR_UID_HIGH, &s->uid_high, SSC_SENSOR_UID_LOW, &s->uid_low,
		SSC_SENSOR_NAME, &s->name, SSC_SENSOR_VENDOR, &s->vendor, NULL);
	s->report_id = g_signal_connect(s->client, "report", G_CALLBACK(report_received), s);
	ssc_sensor_open(s->sensor, NULL, sensor_opened, s);
}

static gboolean
quit_signal(gpointer user_data)
{
	g_main_loop_quit(((Service *)user_data)->loop);
	return G_SOURCE_REMOVE;
}

int
main(void)
{
	Service s = { .inhibitor_fd = -1 };
	g_autoptr(GError) error = NULL;
	s.loop = g_main_loop_new(NULL, FALSE);
	load_configuration(&s);
	load_hold_awake_enabled(&s);
	s.bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!s.bus) g_error("cannot connect to system bus: %s", error->message);
	s.introspection = g_dbus_node_info_new_for_xml(introspection_xml, &error);
	if (!s.introspection) g_error("invalid introspection XML: %s", error->message);
	s.registration_id = g_dbus_connection_register_object(s.bus, OBJECT_PATH,
		s.introspection->interfaces[0], &vtable, &s, NULL, &error);
	if (!s.registration_id) g_error("cannot export SAR object: %s", error->message);
	s.owner_id = g_bus_own_name_on_connection(s.bus, BUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);
	s.stale_timer_id = g_timeout_add_seconds(1, check_sample_freshness, &s);
	g_unix_signal_add(SIGTERM, quit_signal, &s);
	g_unix_signal_add(SIGINT, quit_signal, &s);
	ssc_sensor_new("sar_sensor", NULL, sensor_ready, &s);
	g_main_loop_run(s.loop);
	if (s.stale_timer_id) g_source_remove(s.stale_timer_id);
	release_inhibitor(&s);
	if (s.report_id) g_signal_handler_disconnect(s.client, s.report_id);
	g_clear_object(&s.client);
	g_clear_object(&s.sensor);
	g_clear_pointer(&s.name, g_free);
	g_clear_pointer(&s.vendor, g_free);
	g_bus_unown_name(s.owner_id);
	g_dbus_connection_unregister_object(s.bus, s.registration_id);
	g_dbus_node_info_unref(s.introspection);
	g_object_unref(s.bus);
	g_main_loop_unref(s.loop);
	return 0;
}
