#pragma once

#include <glib.h>

#include "sar-parser.h"

G_BEGIN_DECLS

typedef enum {
	NABU_SAR_SAMPLE_QUALITY_WARMING_UP,
	NABU_SAR_SAMPLE_QUALITY_VALID_CHANGING,
	NABU_SAR_SAMPLE_QUALITY_INVALID_SATURATED,
	NABU_SAR_SAMPLE_QUALITY_STUCK_CONSTANT,
	NABU_SAR_SAMPLE_QUALITY_STUCK_SATURATED,
	NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE,
} NabuSarSampleQuality;

typedef struct NabuSarHealth NabuSarHealth;

NabuSarHealth *nabu_sar_health_new(void);
void nabu_sar_health_free(NabuSarHealth *health);
void nabu_sar_health_reset(NabuSarHealth *health);
NabuSarSampleQuality nabu_sar_health_update(NabuSarHealth *health,
					     const NabuSarSample *sample);
void nabu_sar_health_mark_transport_stale(NabuSarHealth *health);
NabuSarSampleQuality nabu_sar_health_quality(const NabuSarHealth *health);
const gchar *nabu_sar_sample_quality_to_string(NabuSarSampleQuality quality);
gboolean nabu_sar_health_data_usable(const NabuSarHealth *health);
gboolean nabu_sar_health_data_changing(const NabuSarHealth *health);
guint nabu_sar_health_consecutive_identical(const NabuSarHealth *health);
guint nabu_sar_health_saturated_channel_mask(const NabuSarHealth *health);

G_END_DECLS
