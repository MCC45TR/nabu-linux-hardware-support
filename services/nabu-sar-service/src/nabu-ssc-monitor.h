#pragma once

#include <gio/gio.h>

typedef struct _NabuSscMonitor NabuSscMonitor;

NabuSscMonitor *nabu_ssc_monitor_new(GDBusConnection *bus, GError **error);
void nabu_ssc_monitor_start(NabuSscMonitor *monitor);
void nabu_ssc_monitor_free(NabuSscMonitor *monitor);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(NabuSscMonitor, nabu_ssc_monitor_free)
