#pragma once

#include <glib.h>

#define NABU_SAR_VALUE_COUNT 9
#define NABU_SAR_CHANNEL_COUNT 3

typedef struct {
	gfloat values[NABU_SAR_VALUE_COUNT];
	gfloat delta[NABU_SAR_CHANNEL_COUNT];
	gfloat raw[NABU_SAR_CHANNEL_COUNT];
	gfloat baseline[NABU_SAR_CHANNEL_COUNT];
	guint accuracy;
} NabuSarSample;

gboolean nabu_sar_parse_report(const guint8 *data, gsize length,
			       NabuSarSample *sample, GError **error);
