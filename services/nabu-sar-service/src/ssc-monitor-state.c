#include "ssc-monitor-state.h"

/* These are Sensor Core control replies, not sensor-produced reports. */
#define SSC_MSG_RESPONSE_GET_ATTRIBUTES 128
#define SSC_MSG_RESPONSE_ENABLE_REPORT 768
#define SSC_MSG_REPORT_PROXIMITY 769
#define SSC_MSG_REPORT_MOTION_DETECT 771
#define SSC_MSG_REPORT_STANDARD 1025

gboolean
nabu_ssc_message_is_sensor_report(guint32 message_id)
{
	/*
	 * Count only sensor data message IDs whose meaning is known.  In
	 * particular, an SSC error/configuration message addressed to the same
	 * SUID must never be presented as proof of event delivery.
	 */
	switch (message_id) {
	case SSC_MSG_REPORT_PROXIMITY:
	case SSC_MSG_REPORT_MOTION_DETECT:
	case SSC_MSG_REPORT_STANDARD:
		return TRUE;
	case SSC_MSG_RESPONSE_GET_ATTRIBUTES:
	case SSC_MSG_RESPONSE_ENABLE_REPORT:
	default:
		return FALSE;
	}
}

gboolean
nabu_ssc_stream_type_is_safe_to_monitor(guint stream_type)
{
	/* Never enable a permanent, unspecified-rate continuous stream. */
	return stream_type == 1 || stream_type == 2;
}

gboolean
nabu_ssc_monitor_state_record_report(NabuSscMonitorState *state,
				       guint32 message_id,
				       gint64 now_usec)
{
	g_return_val_if_fail(state != NULL, FALSE);

	if (!nabu_ssc_message_is_sensor_report(message_id))
		return FALSE;

	state->report_observed = TRUE;
	if (state->report_count < G_MAXUINT64)
		state->report_count++;
	state->last_report_message_id = message_id;
	state->last_report_usec = now_usec;
	state->stage = NABU_SSC_STAGE_MONITORING;
	return TRUE;
}

gboolean
nabu_ssc_monitor_state_report_fresh(const NabuSscMonitorState *state,
				      gint64 now_usec,
				      gint64 freshness_usec)
{
	g_return_val_if_fail(state != NULL, FALSE);

	return state->report_observed && freshness_usec > 0 &&
		now_usec >= state->last_report_usec &&
		now_usec - state->last_report_usec <= freshness_usec;
}

const gchar *
nabu_ssc_monitor_state_status(const NabuSscMonitorState *state)
{
	g_return_val_if_fail(state != NULL, "unavailable");

	if (state->report_observed)
		return "reporting";

	switch (state->stage) {
	case NABU_SSC_STAGE_QUEUED: return "queued";
	case NABU_SSC_STAGE_DISCOVERING: return "discovering";
	case NABU_SSC_STAGE_DISCOVERED: return "discovered";
	case NABU_SSC_STAGE_ENABLING: return "enabling";
	case NABU_SSC_STAGE_MONITORING: return "monitoring";
	case NABU_SSC_STAGE_INCOMPATIBLE_ABI: return "incompatible-libssc-abi";
	case NABU_SSC_STAGE_UNSUPPORTED_STREAM: return "unsupported-stream";
	case NABU_SSC_STAGE_TIMEOUT: return "timeout";
	case NABU_SSC_STAGE_ENABLE_FAILED: return "enable-failed";
	case NABU_SSC_STAGE_UNAVAILABLE:
	default:
		return "unavailable";
	}
}

const gchar *
nabu_ssc_stream_type_to_string(guint stream_type)
{
	switch (stream_type) {
	case 0: return "continuous";
	case 1: return "on-change";
	case 2: return "single-output";
	default: return "unknown";
	}
}
