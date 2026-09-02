#include "sar-state.h"

#include <math.h>

NabuSarState
nabu_sar_classifier_update(NabuSarClassifier *classifier,
			   const NabuSarSample *sample)
{
	gfloat peak = 0.0f;
	NabuSarState candidate;

	if (!classifier->enabled)
		return classifier->state = NABU_SAR_STATE_UNKNOWN;

	for (guint i = 0; i < NABU_SAR_CHANNEL_COUNT; i++) {
		if (classifier->channel_mask & (1U << i))
			peak = MAX(peak, fabsf(sample->delta[i]));
	}

	if (classifier->state == NABU_SAR_STATE_HELD)
		candidate = peak <= classifier->released_threshold ?
			NABU_SAR_STATE_RELEASED : NABU_SAR_STATE_HELD;
	else
		candidate = peak >= classifier->held_threshold ?
			NABU_SAR_STATE_HELD : NABU_SAR_STATE_RELEASED;

	if (candidate == classifier->state) {
		classifier->candidate_count = 0;
		return classifier->state;
	}
	if (++classifier->candidate_count >= MAX(classifier->debounce_samples, 1U)) {
		classifier->state = candidate;
		classifier->candidate_count = 0;
	}
	return classifier->state;
}

const gchar *
nabu_sar_state_to_string(NabuSarState state)
{
	switch (state) {
	case NABU_SAR_STATE_RELEASED: return "released";
	case NABU_SAR_STATE_HELD: return "held";
	default: return "unknown";
	}
}

gboolean
nabu_sar_should_inhibit(gboolean hold_awake_enabled,
			gboolean mapping_enabled,
			gboolean sample_fresh,
			NabuSarState state)
{
	return hold_awake_enabled && mapping_enabled && sample_fresh &&
		state == NABU_SAR_STATE_HELD;
}
