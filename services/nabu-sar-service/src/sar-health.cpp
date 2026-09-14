#include "sar-health.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
constexpr float kEqualityEpsilon = 0.25F;
constexpr float kAdcHighSaturation = 65534.0F;
constexpr guint kChangesForUsableData = 2;
constexpr guint kSaturatedStuckSamples = 16;
constexpr guint kConstantStuckSamples = 32;

bool nearlyEqual(float left, float right)
{
	return std::abs(left - right) <= kEqualityEpsilon;
}

bool sameSample(const NabuSarSample &left, const NabuSarSample &right)
{
	for (guint channel = 0; channel < NABU_SAR_CHANNEL_COUNT; ++channel) {
		if (!nearlyEqual(left.delta[channel], right.delta[channel]) ||
		    !nearlyEqual(left.raw[channel], right.raw[channel]) ||
		    !nearlyEqual(left.baseline[channel], right.baseline[channel]))
			return false;
	}
	return true;
}

guint saturatedMask(const NabuSarSample &sample)
{
	guint mask = 0;
	for (guint channel = 0; channel < NABU_SAR_CHANNEL_COUNT; ++channel) {
		if (sample.raw[channel] >= kAdcHighSaturation)
			mask |= 1U << channel;
	}
	return mask;
}
}

struct NabuSarHealth {
	NabuSarSample previous{};
	NabuSarSampleQuality quality = NABU_SAR_SAMPLE_QUALITY_WARMING_UP;
	guint consecutiveIdentical = 0;
	guint observedChanges = 0;
	guint saturatedChannelMask = 0;
	bool hasPrevious = false;
};

extern "C" NabuSarHealth *
nabu_sar_health_new(void)
{
	return new NabuSarHealth;
}

extern "C" void
nabu_sar_health_free(NabuSarHealth *health)
{
	delete health;
}

extern "C" void
nabu_sar_health_reset(NabuSarHealth *health)
{
	if (health)
		*health = NabuSarHealth{};
}

extern "C" NabuSarSampleQuality
nabu_sar_health_update(NabuSarHealth *health, const NabuSarSample *sample)
{
	g_return_val_if_fail(health != nullptr, NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE);
	g_return_val_if_fail(sample != nullptr, NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE);

	health->saturatedChannelMask = saturatedMask(*sample);
	if (!health->hasPrevious) {
		health->previous = *sample;
		health->hasPrevious = true;
		health->quality = NABU_SAR_SAMPLE_QUALITY_WARMING_UP;
		return health->quality;
	}

	if (sameSample(health->previous, *sample)) {
		if (health->consecutiveIdentical < std::numeric_limits<guint>::max())
			++health->consecutiveIdentical;
	} else {
		health->consecutiveIdentical = 0;
		if (health->observedChanges < std::numeric_limits<guint>::max())
			++health->observedChanges;
		health->previous = *sample;
	}

	if (health->saturatedChannelMask != 0)
		health->quality = health->consecutiveIdentical >= kSaturatedStuckSamples
			? NABU_SAR_SAMPLE_QUALITY_STUCK_SATURATED
			: NABU_SAR_SAMPLE_QUALITY_INVALID_SATURATED;
	else if (health->consecutiveIdentical >= kConstantStuckSamples)
		health->quality = NABU_SAR_SAMPLE_QUALITY_STUCK_CONSTANT;
	else if (health->observedChanges >= kChangesForUsableData &&
		 health->saturatedChannelMask == 0)
		health->quality = NABU_SAR_SAMPLE_QUALITY_VALID_CHANGING;
	else
		health->quality = NABU_SAR_SAMPLE_QUALITY_WARMING_UP;

	return health->quality;
}

extern "C" void
nabu_sar_health_mark_transport_stale(NabuSarHealth *health)
{
	if (health)
		health->quality = NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE;
}

extern "C" NabuSarSampleQuality
nabu_sar_health_quality(const NabuSarHealth *health)
{
	return health ? health->quality : NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE;
}

extern "C" const gchar *
nabu_sar_sample_quality_to_string(NabuSarSampleQuality quality)
{
	switch (quality) {
	case NABU_SAR_SAMPLE_QUALITY_WARMING_UP: return "warming-up";
	case NABU_SAR_SAMPLE_QUALITY_VALID_CHANGING: return "valid-changing";
	case NABU_SAR_SAMPLE_QUALITY_INVALID_SATURATED: return "invalid-saturated";
	case NABU_SAR_SAMPLE_QUALITY_STUCK_CONSTANT: return "stuck-constant";
	case NABU_SAR_SAMPLE_QUALITY_STUCK_SATURATED: return "stuck-saturated";
	case NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE: return "transport-stale";
	}
	return "transport-stale";
}

extern "C" gboolean
nabu_sar_health_data_usable(const NabuSarHealth *health)
{
	return health && health->quality == NABU_SAR_SAMPLE_QUALITY_VALID_CHANGING;
}

extern "C" gboolean
nabu_sar_health_data_changing(const NabuSarHealth *health)
{
	return health && health->observedChanges > 0 &&
		health->quality != NABU_SAR_SAMPLE_QUALITY_STUCK_CONSTANT &&
		health->quality != NABU_SAR_SAMPLE_QUALITY_STUCK_SATURATED &&
		health->quality != NABU_SAR_SAMPLE_QUALITY_TRANSPORT_STALE;
}

extern "C" guint
nabu_sar_health_consecutive_identical(const NabuSarHealth *health)
{
	return health ? health->consecutiveIdentical : 0;
}

extern "C" guint
nabu_sar_health_saturated_channel_mask(const NabuSarHealth *health)
{
	return health ? health->saturatedChannelMask : 0;
}
