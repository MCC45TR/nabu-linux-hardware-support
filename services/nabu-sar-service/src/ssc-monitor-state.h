#pragma once

#include <glib.h>

typedef enum {
	NABU_SSC_STAGE_QUEUED,
	NABU_SSC_STAGE_DISCOVERING,
	NABU_SSC_STAGE_DISCOVERED,
	NABU_SSC_STAGE_ENABLING,
	NABU_SSC_STAGE_MONITORING,
	NABU_SSC_STAGE_UNAVAILABLE,
	NABU_SSC_STAGE_INCOMPATIBLE_ABI,
	NABU_SSC_STAGE_UNSUPPORTED_STREAM,
	NABU_SSC_STAGE_TIMEOUT,
	NABU_SSC_STAGE_ENABLE_FAILED,
} NabuSscStage;

typedef struct {
	NabuSscStage stage;
	gboolean report_observed;
	guint64 report_count;
	guint32 last_report_message_id;
	gint64 last_report_usec;
} NabuSscMonitorState;

gboolean nabu_ssc_message_is_sensor_report(guint32 message_id);
gboolean nabu_ssc_stream_type_is_safe_to_monitor(guint stream_type);
gboolean nabu_ssc_monitor_state_record_report(NabuSscMonitorState *state,
					       guint32 message_id,
					       gint64 now_usec);
gboolean nabu_ssc_monitor_state_report_fresh(const NabuSscMonitorState *state,
					      gint64 now_usec,
					      gint64 freshness_usec);
const gchar *nabu_ssc_monitor_state_status(const NabuSscMonitorState *state);
const gchar *nabu_ssc_stream_type_to_string(guint stream_type);
