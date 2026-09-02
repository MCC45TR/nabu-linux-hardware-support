#pragma once

#include "sar-parser.h"

typedef enum {
	NABU_SAR_STATE_UNKNOWN,
	NABU_SAR_STATE_RELEASED,
	NABU_SAR_STATE_HELD,
} NabuSarState;

typedef struct {
	gboolean enabled;
	guint channel_mask;
	gfloat held_threshold;
	gfloat released_threshold;
	guint debounce_samples;
	guint candidate_count;
	NabuSarState state;
} NabuSarClassifier;

NabuSarState nabu_sar_classifier_update(NabuSarClassifier *classifier,
					const NabuSarSample *sample);
const gchar *nabu_sar_state_to_string(NabuSarState state);
gboolean nabu_sar_should_inhibit(gboolean hold_awake_enabled,
				 gboolean mapping_enabled,
				 gboolean sample_fresh,
				 NabuSarState state);
