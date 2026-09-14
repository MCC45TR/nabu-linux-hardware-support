#include <glib.h>
#include <math.h>

#include "sar-parser.h"
#include "sar-health.h"
#include "sar-state.h"
#include "ssc-monitor-state.h"

static void
test_health_rejects_fixed_saturated_stream(void)
{
	NabuSarSample sample = {
		.delta = { 25535.0f, 0.0f, 25535.0f },
		.raw = { 65535.0f, 0.0f, 65535.0f },
		.baseline = { 40000.0f, 0.0f, 40000.0f },
	};
	NabuSarHealth *health = nabu_sar_health_new();

	g_assert_cmpint(nabu_sar_health_update(health, &sample), ==,
			NABU_SAR_SAMPLE_QUALITY_WARMING_UP);
	g_assert_cmpint(nabu_sar_health_update(health, &sample), ==,
			NABU_SAR_SAMPLE_QUALITY_INVALID_SATURATED);
	for (guint i = 0; i < 15; ++i)
		nabu_sar_health_update(health, &sample);
	g_assert_cmpint(nabu_sar_health_quality(health), ==,
			NABU_SAR_SAMPLE_QUALITY_STUCK_SATURATED);
	g_assert_false(nabu_sar_health_data_usable(health));
	g_assert_false(nabu_sar_health_data_changing(health));
	g_assert_cmpuint(nabu_sar_health_saturated_channel_mask(health), ==, 5);
	g_assert_cmpuint(nabu_sar_health_consecutive_identical(health), ==, 16);
	nabu_sar_health_free(health);
}

static void
test_health_accepts_only_validated_variation(void)
{
	NabuSarSample sample = {
		.delta = { -50.0f, 0.0f, -50.0f },
		.raw = { 19893.0f, 0.0f, 19893.0f },
		.baseline = { 19943.0f, 0.0f, 19943.0f },
	};
	NabuSarHealth *health = nabu_sar_health_new();

	nabu_sar_health_update(health, &sample);
	sample.delta[0] = -48.0f;
	sample.raw[0] = 19895.0f;
	nabu_sar_health_update(health, &sample);
	g_assert_false(nabu_sar_health_data_usable(health));
	sample.delta[2] = -47.0f;
	sample.raw[2] = 19896.0f;
	g_assert_cmpint(nabu_sar_health_update(health, &sample), ==,
			NABU_SAR_SAMPLE_QUALITY_VALID_CHANGING);
	g_assert_true(nabu_sar_health_data_usable(health));
	g_assert_true(nabu_sar_health_data_changing(health));
	nabu_sar_health_mark_transport_stale(health);
	g_assert_cmpstr(nabu_sar_sample_quality_to_string(
			nabu_sar_health_quality(health)), ==, "transport-stale");
	g_assert_false(nabu_sar_health_data_usable(health));
	nabu_sar_health_free(health);
}

static void
test_parse_live_shape(void)
{
	const guint8 report[] = {
		0x0a, 0x24,
		0x00,0x00,0x48,0xc2, 0x00,0x6a,0x9b,0x46, 0x00,0xce,0x9b,0x46,
		0,0,0,0, 0,0,0,0, 0,0,0,0,
		0x00,0x00,0x48,0xc2, 0x00,0x6a,0x9b,0x46, 0x00,0xce,0x9b,0x46,
		0x10, 0x03
	};
	NabuSarSample sample;
	g_autoptr(GError) error = NULL;

	g_assert_true(nabu_sar_parse_report(report, sizeof(report), &sample, &error));
	g_assert_no_error(error);
	g_assert_cmpfloat(sample.delta[0], ==, -50.0f);
	g_assert_cmpfloat(sample.raw[0], ==, 19893.0f);
	g_assert_cmpfloat(sample.baseline[0], ==, 19943.0f);
	g_assert_cmpfloat(sample.delta[2], ==, -50.0f);
	g_assert_cmpuint(sample.accuracy, ==, 3);
}

static void
test_classifier_is_fail_closed(void)
{
	NabuSarSample sample = { .delta = { 900.0f, 0.0f, 0.0f } };
	NabuSarClassifier classifier = {
		.enabled = FALSE, .channel_mask = 1,
		.held_threshold = 500.0f, .released_threshold = 250.0f,
		.debounce_samples = 3,
	};

	g_assert_cmpint(nabu_sar_classifier_update(&classifier, &sample), ==,
			NABU_SAR_STATE_UNKNOWN);
	classifier.enabled = TRUE;
	g_assert_cmpint(nabu_sar_classifier_update(&classifier, &sample), ==,
			NABU_SAR_STATE_UNKNOWN);
	g_assert_cmpint(nabu_sar_classifier_update(&classifier, &sample), ==,
			NABU_SAR_STATE_UNKNOWN);
	g_assert_cmpint(nabu_sar_classifier_update(&classifier, &sample), ==,
			NABU_SAR_STATE_HELD);
}

static void
test_classifier_rejects_unsafe_configuration(void)
{
	NabuSarClassifier classifier = {
		.enabled = TRUE, .channel_mask = 1,
		.held_threshold = 500.0f, .released_threshold = 250.0f,
		.debounce_samples = 3,
	};
	NabuSarSample sample = { .delta = { 900.0f, 0.0f, 0.0f } };

	g_assert_true(nabu_sar_classifier_configuration_is_valid(&classifier));
	classifier.channel_mask = 1U << NABU_SAR_CHANNEL_COUNT;
	g_assert_false(nabu_sar_classifier_configuration_is_valid(&classifier));
	classifier.channel_mask = 1;
	classifier.released_threshold = -1.0f;
	g_assert_false(nabu_sar_classifier_configuration_is_valid(&classifier));
	classifier.released_threshold = 250.0f;
	classifier.held_threshold = INFINITY;
	g_assert_false(nabu_sar_classifier_configuration_is_valid(&classifier));
	g_assert_cmpint(nabu_sar_classifier_update(&classifier, &sample), ==,
			NABU_SAR_STATE_UNKNOWN);
	g_assert_false(classifier.enabled);
}

static void
test_parser_rejects_non_finite_values(void)
{
	const guint8 report[] = {
		0x0a, 0x24,
		0x00,0x00,0xc0,0x7f, 0,0,0,0, 0,0,0,0,
		0,0,0,0, 0,0,0,0, 0,0,0,0,
		0,0,0,0, 0,0,0,0, 0,0,0,0,
	};
	NabuSarSample sample;
	g_autoptr(GError) error = NULL;

	g_assert_false(nabu_sar_parse_report(report, sizeof(report), &sample, &error));
	g_assert_error(error, g_quark_from_static_string("nabu-sar-parser-error"), 3);
}

static void
test_algorithm_report_state_is_evidence_based(void)
{
	NabuSscMonitorState state = {
		.stage = NABU_SSC_STAGE_ENABLING,
		.report_count = G_MAXUINT64,
	};

	/* Attribute and enable acknowledgements are not hardware reports. */
	g_assert_false(nabu_ssc_monitor_state_record_report(&state, 128, 10));
	g_assert_false(nabu_ssc_monitor_state_record_report(&state, 768, 10));
	g_assert_false(nabu_ssc_monitor_state_record_report(&state, 1026, 10));
	g_assert_false(state.report_observed);
	g_assert_cmpstr(nabu_ssc_monitor_state_status(&state), ==, "enabling");

	/* Message 771 is the observed LSM6DSO motion-detect report. */
	g_assert_true(nabu_ssc_monitor_state_record_report(&state, 771, 20));
	g_assert_true(state.report_observed);
	g_assert_cmpuint(state.report_count, ==, G_MAXUINT64);
	g_assert_cmpuint(state.last_report_message_id, ==, 771);
	g_assert_cmpstr(nabu_ssc_monitor_state_status(&state), ==, "reporting");
	g_assert_true(nabu_ssc_monitor_state_report_fresh(&state, 80, 60));
	g_assert_false(nabu_ssc_monitor_state_report_fresh(&state, 81, 60));
}

static void
test_algorithm_stream_policy_is_fail_closed(void)
{
	g_assert_false(nabu_ssc_stream_type_is_safe_to_monitor(0));
	g_assert_true(nabu_ssc_stream_type_is_safe_to_monitor(1));
	g_assert_true(nabu_ssc_stream_type_is_safe_to_monitor(2));
	g_assert_false(nabu_ssc_stream_type_is_safe_to_monitor(3));
	g_assert_false(nabu_ssc_stream_type_is_safe_to_monitor(G_MAXUINT));
}

static void
test_inhibitor_gate_requires_every_condition(void)
{
	g_assert_false(nabu_sar_should_inhibit(FALSE, TRUE, TRUE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, FALSE, TRUE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, FALSE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_RELEASED));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_UNKNOWN));
	g_assert_true(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_HELD));
}

static void
test_publish_gate_bounds_unchanged_telemetry(void)
{
	g_assert_true(nabu_sar_should_publish(FALSE, NABU_SAR_STATE_UNKNOWN,
			NABU_SAR_STATE_UNKNOWN, 100, 90, 1000));
	g_assert_true(nabu_sar_should_publish(TRUE, NABU_SAR_STATE_RELEASED,
			NABU_SAR_STATE_HELD, 100, 90, 1000));
	g_assert_false(nabu_sar_should_publish(TRUE, NABU_SAR_STATE_HELD,
			NABU_SAR_STATE_HELD, 1099, 100, 1000));
	g_assert_true(nabu_sar_should_publish(TRUE, NABU_SAR_STATE_HELD,
			NABU_SAR_STATE_HELD, 1100, 100, 1000));
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/nabu-sar/parser/live-shape", test_parse_live_shape);
	g_test_add_func("/nabu-sar/parser/reject-non-finite", test_parser_rejects_non_finite_values);
	g_test_add_func("/nabu-sar/health/reject-fixed-saturated", test_health_rejects_fixed_saturated_stream);
	g_test_add_func("/nabu-sar/health/validated-variation", test_health_accepts_only_validated_variation);
	g_test_add_func("/nabu-sar/classifier/fail-closed", test_classifier_is_fail_closed);
	g_test_add_func("/nabu-sar/classifier/reject-unsafe-configuration",
			test_classifier_rejects_unsafe_configuration);
	g_test_add_func("/nabu-sar/inhibitor/all-gates", test_inhibitor_gate_requires_every_condition);
	g_test_add_func("/nabu-sar/publish/bounded-unchanged-telemetry",
			test_publish_gate_bounds_unchanged_telemetry);
	g_test_add_func("/nabu-ssc/report/evidence-based-state",
			test_algorithm_report_state_is_evidence_based);
	g_test_add_func("/nabu-ssc/policy/event-driven-only",
			test_algorithm_stream_policy_is_fail_closed);
	return g_test_run();
}
