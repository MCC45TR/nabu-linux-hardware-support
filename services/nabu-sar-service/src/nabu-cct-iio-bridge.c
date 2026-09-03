#include <errno.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <libssc/libssc-sensor-light.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define IIO_DEVICES_PATH "/sys/bus/iio/devices"
#define CCT_MIN_KELVIN 1000.0f
#define CCT_MAX_KELVIN 40000.0f
#define CCT_STALE_USEC (3 * G_USEC_PER_SEC)
#define CCT_INVALID_WARNING_USEC (30 * G_USEC_PER_SEC)

typedef struct {
	GMainLoop *loop;
	SSCSensorLight *sensor;
	gchar *iio_cct_path;
	guint stale_timer_id;
	gint exit_status;
	gint64 last_sample_usec;
	gint64 last_invalid_warning_usec;
	guint invalid_samples_suppressed;
} Bridge;

static gboolean
publish_kelvin(const gchar *path, gint kelvin, GError **error)
{
	gchar value[32];
	gsize length = g_snprintf(value, sizeof(value), "%d\n", kelvin);
	gsize offset = 0;
	gint fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
			    "cannot open %s: %s", path, g_strerror(errno));
		return FALSE;
	}
	while (offset < length) {
		ssize_t written = write(fd, value + offset, length - offset);
		if (written < 0) {
			g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
				    "cannot write %s: %s", path, g_strerror(errno));
			close(fd);
			return FALSE;
		}
		offset += written;
	}
	close(fd);
	return TRUE;
}

static gchar *
find_iio_cct_path(void)
{
	g_autoptr(GDir) directory = g_dir_open(IIO_DEVICES_PATH, 0, NULL);
	const gchar *entry;

	if (!directory)
		return NULL;
	while ((entry = g_dir_read_name(directory))) {
		if (!g_str_has_prefix(entry, "iio:device"))
			continue;
		g_autofree gchar *name_path = g_build_filename(IIO_DEVICES_PATH,
			entry, "name", NULL);
		g_autofree gchar *name = NULL;
		if (!g_file_get_contents(name_path, &name, NULL, NULL))
			continue;
		g_strchomp(name);
		if (strcmp(name, "nabu-cct"))
			continue;
		return g_build_filename(IIO_DEVICES_PATH, entry,
			"in_colortemp_raw", NULL);
	}
	return NULL;
}

static void
measurement(SSCSensorLight *sensor, gfloat kelvin, gpointer user_data)
{
	Bridge *bridge = user_data;
	g_autoptr(GError) error = NULL;

	if (!isfinite(kelvin) || kelvin < CCT_MIN_KELVIN ||
	    kelvin > CCT_MAX_KELVIN) {
		gint64 now = g_get_monotonic_time();

		if (!bridge->last_invalid_warning_usec ||
		    now - bridge->last_invalid_warning_usec >=
		    CCT_INVALID_WARNING_USEC) {
			g_warning("discarding invalid TCS3701 CCT measurement: %.9g "
				  "(%u similar samples suppressed)", kelvin,
				  bridge->invalid_samples_suppressed);
			bridge->last_invalid_warning_usec = now;
			bridge->invalid_samples_suppressed = 0;
		} else if (bridge->invalid_samples_suppressed < G_MAXUINT) {
			bridge->invalid_samples_suppressed++;
		}
		return;
	}
	if (!publish_kelvin(bridge->iio_cct_path, (gint)lroundf(kelvin), &error))
		g_warning("cannot publish CCT through IIO: %s", error->message);
	else
		bridge->last_sample_usec = g_get_monotonic_time();
}

static gboolean
check_freshness(gpointer user_data)
{
	Bridge *bridge = user_data;
	g_autoptr(GError) error = NULL;

	if (!bridge->last_sample_usec ||
	    g_get_monotonic_time() - bridge->last_sample_usec <= CCT_STALE_USEC)
		return G_SOURCE_CONTINUE;
	if (!publish_kelvin(bridge->iio_cct_path, 0, &error))
		g_warning("cannot invalidate stale CCT measurement: %s", error->message);
	else
		g_warning("CCT stream became stale; standard IIO value invalidated");
	bridge->last_sample_usec = 0;
	return G_SOURCE_CONTINUE;
}

static void
sensor_opened(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Bridge *bridge = user_data;
	g_autoptr(GError) error = NULL;

	if (!ssc_sensor_light_open_finish(bridge->sensor, result, &error)) {
		g_warning("cannot enable TCS3701 cct_front_strm stream: %s", error->message);
		bridge->exit_status = 1;
		g_main_loop_quit(bridge->loop);
		return;
	}
	g_message("publishing TCS3701 cct_front through standard IIO colour temperature");
}

static void
sensor_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	Bridge *bridge = user_data;
	g_autoptr(GError) error = NULL;
	GObject *object = g_async_initable_new_finish(G_ASYNC_INITABLE(source),
		result, &error);

	if (!object) {
		g_warning("TCS3701 cct_front_strm unavailable: %s", error->message);
		bridge->exit_status = 1;
		g_main_loop_quit(bridge->loop);
		return;
	}
	bridge->sensor = SSC_SENSOR_LIGHT(object);
	g_signal_connect(bridge->sensor, "measurement", G_CALLBACK(measurement), bridge);
	ssc_sensor_light_open(bridge->sensor, NULL, sensor_opened, bridge);
}

static gboolean
quit_signal(gpointer user_data)
{
	g_main_loop_quit(((Bridge *)user_data)->loop);
	return G_SOURCE_REMOVE;
}

int
main(void)
{
	Bridge bridge = { 0 };

	bridge.iio_cct_path = find_iio_cct_path();
	if (!bridge.iio_cct_path ||
	    !g_file_test(bridge.iio_cct_path, G_FILE_TEST_EXISTS)) {
		g_printerr("standard nabu-cct IIO endpoint is unavailable\n");
		g_free(bridge.iio_cct_path);
		return 1;
	}
	bridge.loop = g_main_loop_new(NULL, FALSE);
	bridge.stale_timer_id = g_timeout_add_seconds(1, check_freshness, &bridge);
	g_unix_signal_add(SIGTERM, quit_signal, &bridge);
	g_unix_signal_add(SIGINT, quit_signal, &bridge);
	g_async_initable_new_async(SSC_TYPE_SENSOR_LIGHT, G_PRIORITY_DEFAULT,
		NULL, sensor_ready, &bridge,
		SSC_SENSOR_DATA_TYPE, "cct_front_strm", NULL);
	g_main_loop_run(bridge.loop);
	if (bridge.stale_timer_id)
		g_source_remove(bridge.stale_timer_id);
	publish_kelvin(bridge.iio_cct_path, 0, NULL);
	if (bridge.sensor) {
		ssc_sensor_light_close_sync(bridge.sensor, NULL, NULL);
		g_object_unref(bridge.sensor);
	}
	g_free(bridge.iio_cct_path);
	g_main_loop_unref(bridge.loop);
	return bridge.exit_status;
}
