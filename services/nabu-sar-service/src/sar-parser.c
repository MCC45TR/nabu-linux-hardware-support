#include "sar-parser.h"

#include <string.h>

static GQuark
nabu_sar_parser_error_quark(void)
{
	return g_quark_from_static_string("nabu-sar-parser-error");
}

static gboolean
read_varint(const guint8 *data, gsize length, gsize *offset, guint64 *value)
{
	guint shift = 0;
	guint64 result = 0;

	while (*offset < length && shift < 64) {
		guint8 byte = data[(*offset)++];
		result |= ((guint64)(byte & 0x7f)) << shift;
		if (!(byte & 0x80)) {
			*value = result;
			return TRUE;
		}
		shift += 7;
	}
	return FALSE;
}

gboolean
nabu_sar_parse_report(const guint8 *data, gsize length,
			      NabuSarSample *sample, GError **error)
{
	gsize offset = 0;
	const guint8 *payload = NULL;
	gsize payload_length = 0;
	guint accuracy = 0;

	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(sample != NULL, FALSE);
	memset(sample, 0, sizeof(*sample));

	while (offset < length) {
		guint64 key, value;
		guint field, wire;

		if (!read_varint(data, length, &offset, &key))
			goto malformed;
		field = key >> 3;
		wire = key & 7;
		if (field == 1 && wire == 2) {
			if (!read_varint(data, length, &offset, &value) ||
			    value > length - offset)
				goto malformed;
			payload = data + offset;
			payload_length = value;
			offset += value;
		} else if (field == 2 && wire == 0) {
			if (!read_varint(data, length, &offset, &value))
				goto malformed;
			accuracy = value;
		} else if (wire == 0) {
			if (!read_varint(data, length, &offset, &value))
				goto malformed;
		} else if (wire == 2) {
			if (!read_varint(data, length, &offset, &value) ||
			    value > length - offset)
				goto malformed;
			offset += value;
		} else {
			goto malformed;
		}
	}

	if (payload_length != NABU_SAR_VALUE_COUNT * sizeof(gfloat)) {
		g_set_error(error, nabu_sar_parser_error_quark(), 2,
			    "expected 36-byte ADUX1050 payload, got %zu",
			    payload_length);
		return FALSE;
	}

	for (guint i = 0; i < NABU_SAR_VALUE_COUNT; i++) {
		guint32 bits;
		memcpy(&bits, payload + i * sizeof(bits), sizeof(bits));
		bits = GUINT32_FROM_LE(bits);
		memcpy(&sample->values[i], &bits, sizeof(bits));
	}
	for (guint channel = 0; channel < NABU_SAR_CHANNEL_COUNT; channel++) {
		guint base = channel * 3;
		sample->delta[channel] = sample->values[base];
		sample->raw[channel] = sample->values[base + 1];
		sample->baseline[channel] = sample->values[base + 2];
	}
	sample->accuracy = accuracy;
	return TRUE;

malformed:
	g_set_error_literal(error, nabu_sar_parser_error_quark(), 1,
			    "malformed ADUX1050 protobuf report");
	return FALSE;
}
