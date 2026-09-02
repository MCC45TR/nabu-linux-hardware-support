#include <glib.h>

#include "sar-parser.h"
#include "sar-state.h"

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
test_inhibitor_gate_requires_every_condition(void)
{
	g_assert_false(nabu_sar_should_inhibit(FALSE, TRUE, TRUE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, FALSE, TRUE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, FALSE, NABU_SAR_STATE_HELD));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_RELEASED));
	g_assert_false(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_UNKNOWN));
	g_assert_true(nabu_sar_should_inhibit(TRUE, TRUE, TRUE, NABU_SAR_STATE_HELD));
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/nabu-sar/parser/live-shape", test_parse_live_shape);
	g_test_add_func("/nabu-sar/classifier/fail-closed", test_classifier_is_fail_closed);
	g_test_add_func("/nabu-sar/inhibitor/all-gates", test_inhibitor_gate_requires_every_condition);
	return g_test_run();
}
