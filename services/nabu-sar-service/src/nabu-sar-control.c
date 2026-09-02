#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#define BUS_NAME "org.senemos.Nabu.Sar"
#define OBJECT_PATH "/org/senemos/Nabu/Sar"
#define INTERFACE_NAME "org.senemos.Nabu.Sar1"

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
	printf("sample_fresh=%d\n", lookup_boolean(properties, "SampleFresh", FALSE));
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
