#include <gio/gio.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#define BUS_NAME "org.senemos.Nabu.Sar"
#define OBJECT_PATH "/org/senemos/Nabu/Sar"
#define INTERFACE_NAME "org.senemos.Nabu.Sar1"
#define SENSORS_OBJECT_PATH "/org/senemos/Nabu/Sensors"
#define SENSORS_INTERFACE_NAME "org.senemos.Nabu.Sensors1"

static gboolean
lookup_boolean(GVariant *properties, const gchar *name, gboolean fallback)
{
	g_autoptr(GVariant) value = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE_BOOLEAN);
	return value ? g_variant_get_boolean(value) : fallback;
}

static gchar *
lookup_string(GVariant *properties, const gchar *name, const gchar *fallback)
{
	g_autoptr(GVariant) value = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE_STRING);
	return value ? g_variant_dup_string(value, NULL) : g_strdup(fallback);
}

static guint32
lookup_uint32(GVariant *properties, const gchar *name, guint32 fallback)
{
	g_autoptr(GVariant) value = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE_UINT32);
	return value ? g_variant_get_uint32(value) : fallback;
}

static guint64
lookup_uint64(GVariant *properties, const gchar *name, guint64 fallback)
{
	g_autoptr(GVariant) value = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE_UINT64);
	return value ? g_variant_get_uint64(value) : fallback;
}

static gdouble
lookup_double(GVariant *properties, const gchar *name, gdouble fallback)
{
	g_autoptr(GVariant) value = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE_DOUBLE);
	return value ? g_variant_get_double(value) : fallback;
}

static void
print_array(GVariant *properties, const gchar *name)
{
	g_autoptr(GVariant) values = g_variant_lookup_value(properties, name,
		G_VARIANT_TYPE("ad"));
	gsize count = values ? g_variant_n_children(values) : 0;
	for (gsize i = 0; i < count; i++) {
		gdouble value;
		g_variant_get_child(values, i, "d", &value);
		printf("%s%.9g", i ? "," : "", value);
	}
}

static void
append_csv(GString *list, const gchar *value)
{
	if (list->len)
		g_string_append_c(list, ',');
	g_string_append(list, value);
}

static void
print_algorithm_status(GDBusConnection *bus)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) reply = g_dbus_connection_call_sync(bus, BUS_NAME,
		SENSORS_OBJECT_PATH, "org.freedesktop.DBus.Properties", "GetAll",
		g_variant_new("(s)", SENSORS_INTERFACE_NAME),
		G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, 5000,
		NULL, &error);
	if (!reply) {
		printf("algorithm_service=0\n");
		return;
	}

	g_autoptr(GVariant) properties = g_variant_get_child_value(reply, 0);
	g_autoptr(GVariant) algorithms = g_variant_lookup_value(properties,
		"Algorithms", G_VARIANT_TYPE("aa{sv}"));
	g_autoptr(GString) discovered = g_string_new(NULL);
	g_autoptr(GString) available = g_string_new(NULL);
	g_autoptr(GString) monitoring = g_string_new(NULL);
	g_autoptr(GString) reports = g_string_new(NULL);
	g_autoptr(GString) fresh = g_string_new(NULL);
	g_autoptr(GString) waiting = g_string_new(NULL);
	g_autoptr(GString) discovered_only = g_string_new(NULL);
	g_autoptr(GString) unavailable = g_string_new(NULL);
	guint total = 0, discovered_count = 0, available_count = 0;
	guint monitoring_count = 0;
	guint reporting_count = 0;

	if (algorithms) {
		GVariantIter iterator;
		GVariant *algorithm;
		g_variant_iter_init(&iterator, algorithms);
		while ((algorithm = g_variant_iter_next_value(&iterator))) {
			g_autofree gchar *data_type = lookup_string(algorithm,
				"DataType", "unknown");
			gboolean was_discovered = lookup_boolean(algorithm,
				"Discovered", FALSE);
			gboolean is_available = lookup_boolean(algorithm,
				"Available", FALSE);
			gboolean is_monitoring = lookup_boolean(algorithm,
				"Monitoring", FALSE);
			gboolean report_observed = lookup_boolean(algorithm,
				"ReportObserved", FALSE);
			gboolean report_fresh = lookup_boolean(algorithm,
				"ReportFresh", FALSE);

			total++;
			if (was_discovered) {
				discovered_count++;
				append_csv(discovered, data_type);
			}
			if (is_available) {
				available_count++;
				append_csv(available, data_type);
			} else {
				append_csv(unavailable, data_type);
			}
			if (is_monitoring) {
				monitoring_count++;
				append_csv(monitoring, data_type);
			}
			if (report_observed) {
				reporting_count++;
				append_csv(reports, data_type);
				if (report_fresh)
					append_csv(fresh, data_type);
			} else if (is_monitoring) {
				append_csv(waiting, data_type);
			} else if (is_available) {
				append_csv(discovered_only, data_type);
			}
			g_variant_unref(algorithm);
		}
	}

	printf("algorithm_service=1\n");
	printf("algorithm_ready=%d\n", lookup_boolean(properties, "Ready", FALSE));
	printf("algorithm_total=%u\n", total);
	printf("algorithm_discovered_count=%u\n", discovered_count);
	printf("algorithm_available_count=%u\n", available_count);
	printf("algorithm_monitoring_count=%u\n", monitoring_count);
	printf("algorithm_reporting_count=%u\n", reporting_count);
	printf("algorithm_total_reports=%" G_GUINT64_FORMAT "\n",
		lookup_uint64(properties, "TotalReports", 0));
	printf("algorithm_discovered=%s\n", discovered->str);
	printf("algorithm_available=%s\n", available->str);
	printf("algorithm_monitoring=%s\n", monitoring->str);
	printf("algorithm_reports=%s\n", reports->str);
	printf("algorithm_fresh=%s\n", fresh->str);
	printf("algorithm_waiting=%s\n", waiting->str);
	printf("algorithm_discovered_only=%s\n", discovered_only->str);
	printf("algorithm_unavailable=%s\n", unavailable->str);
}

static int
status(GDBusConnection *bus)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) reply = g_dbus_connection_call_sync(bus, BUS_NAME,
		OBJECT_PATH, "org.freedesktop.DBus.Properties", "GetAll",
		g_variant_new("(s)", INTERFACE_NAME), G_VARIANT_TYPE("(a{sv})"),
		G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
	if (!reply) {
		g_printerr("nabu-sar-control: %s\n", error->message);
		return 1;
	}

	g_autoptr(GVariant) properties = g_variant_get_child_value(reply, 0);
	g_autofree gchar *grip_state = lookup_string(properties, "GripState", "unknown");
	printf("available=%d\n", lookup_boolean(properties, "Available", FALSE));
	printf("mapping_enabled=%d\n", lookup_boolean(properties, "MappingEnabled", FALSE));
	printf("configured_channel_mask=%u\n", lookup_uint32(properties, "ConfiguredChannelMask", 0));
	printf("held_threshold=%.9g\n", lookup_double(properties, "HeldThreshold", 0.0));
	printf("released_threshold=%.9g\n", lookup_double(properties, "ReleasedThreshold", 0.0));
	printf("debounce_samples=%u\n", lookup_uint32(properties, "DebounceSamples", 0));
	printf("sample_fresh=%d\n", lookup_boolean(properties, "SampleFresh", FALSE));
	printf("sample_sequence=%" G_GUINT64_FORMAT "\n", lookup_uint64(properties, "SampleSequence", 0));
	printf("grip_state=%s\n", grip_state);
	printf("hold_awake_enabled=%d\n", lookup_boolean(properties, "HoldAwakeEnabled", FALSE));
	printf("sleep_inhibited=%d\n", lookup_boolean(properties, "SleepInhibited", FALSE));
	printf("deltas=");
	print_array(properties, "Deltas");
	printf("\nraw_values=");
	print_array(properties, "RawValues");
	printf("\nbaselines=");
	print_array(properties, "Baselines");
	printf("\n");
	print_algorithm_status(bus);
	return 0;
}

static int
set_hold_awake(GDBusConnection *bus, gboolean enabled)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) reply = g_dbus_connection_call_sync(bus, BUS_NAME,
		OBJECT_PATH, INTERFACE_NAME, "SetHoldAwakeEnabled",
		g_variant_new("(b)", enabled), NULL, G_DBUS_CALL_FLAGS_NONE,
		5000, NULL, &error);
	if (!reply) {
		g_printerr("nabu-sar-control: %s\n", error->message);
		return 1;
	}
	return 0;
}

static void
usage(const gchar *program)
{
	g_printerr("Usage: %s status\n"
		"       %s set hold-awake on|off\n", program, program);
}

int
main(int argc, char **argv)
{
	setlocale(LC_NUMERIC, "C");
	if (argc != 2 && argc != 4) {
		usage(argv[0]);
		return 2;
	}

	g_autoptr(GError) error = NULL;
	g_autoptr(GDBusConnection) bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!bus) {
		g_printerr("nabu-sar-control: %s\n", error->message);
		return 1;
	}
	if (argc == 2 && !strcmp(argv[1], "status"))
		return status(bus);
	if (argc == 4 && !strcmp(argv[1], "set") &&
	    !strcmp(argv[2], "hold-awake")) {
		if (!strcmp(argv[3], "on"))
			return set_hold_awake(bus, TRUE);
		if (!strcmp(argv[3], "off"))
			return set_hold_awake(bus, FALSE);
	}
	usage(argv[0]);
	return 2;
}
