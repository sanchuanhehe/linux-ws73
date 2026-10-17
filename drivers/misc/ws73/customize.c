// SPDX-License-Identifier: GPL-2.0-only
/* Independent SDK110 customization wire codec; no vendor defaults. */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "customize.h"

static void ws73_frequency_encode(u8 *out, const struct ws73_frequency_reference *entry)
{
	put_unaligned_le16(entry->reference, out);
	put_unaligned_le16(entry->frequency, out + 2);
}

int ws73_customize_encode(u8 *out, size_t capacity,
			  const struct ws73_customization *parameters)
{
	u8 *body;
	unsigned int i;

	if (!out || !parameters)
		return -EINVAL;
	if (parameters->calibration_count > 8)
		return -ERANGE;
	if (capacity < WS73_CUSTOMIZE_SIZE)
		return -ENOSPC;

	memset(out, 0, WS73_CUSTOMIZE_SIZE);
	out[0] = 0x50; /* Service 5, subtype 0. */
	out[1] = 10;
	/* SDK Host TX passes the entire allocation as HCC pay_len. */
	put_unaligned_le16(WS73_CUSTOMIZE_SIZE, out + 2);
	/* Tag type 0 is customization; length excludes the tag/HCC headers. */
	put_unaligned_le16(WS73_CUSTOMIZE_BODY_SIZE, out + 6);
	body = out + 8;
	put_unaligned_le32(parameters->coexistence, body);
	put_unaligned_le32(parameters->disable_ll_privacy, body + 4);
	put_unaligned_le32(parameters->suspend_mode, body + 8);
	put_unaligned_le32(parameters->scan_interval, body + 12);
	put_unaligned_le32(parameters->scan_window, body + 16);
	put_unaligned_le32(parameters->front_switch, body + 20);
	put_unaligned_le32(parameters->max_power, body + 24);
	put_unaligned_le32(parameters->calibration_count, body + 28);
	for (i = 0; i < 8; i++) {
		ws73_frequency_encode(body + 32 + 4 * i, &parameters->calibration[i]);
		ws73_frequency_encode(body + 84 + 4 * i, &parameters->srrc[i]);
	}
	/* Paired channel boundaries use the first in the HIGH half-word. */
	for (i = 0; i < 6; i += 2) {
		put_unaligned_le16(parameters->channel_boundary[i + 1], body + 64 + 2 * i);
		put_unaligned_le16(parameters->channel_boundary[i], body + 66 + 2 * i);
	}
	put_unaligned_le32(parameters->channel_boundary[6], body + 76);
	put_unaligned_le32(parameters->srrc_switch, body + 80);
	return WS73_CUSTOMIZE_SIZE;
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_encode, "WS73");

int ws73_customize_validate(const u8 *record, size_t length)
{
	if (!record)
		return -EINVAL;
	if (length != WS73_CUSTOMIZE_SIZE)
		return -EMSGSIZE;
	if (record[0] != 0x50 || record[1] != 10)
		return -EPROTO;
	if (get_unaligned_le16(record + 2) != WS73_CUSTOMIZE_SIZE ||
	    get_unaligned_le16(record + 6) != WS73_CUSTOMIZE_BODY_SIZE)
		return -EMSGSIZE;
	if (get_unaligned_le16(record + 4))
		return -EOPNOTSUPP;
	if (get_unaligned_le32(record + 36) > 8)
		return -ERANGE;
	/* Last boundary is one 16-bit value, not a fourth pair. */
	if (get_unaligned_le16(record + 86))
		return -EPROTO;
	/* No flash override, per-device values or nonzero allocation padding. */
	if (memchr_inv(record + 124, 0, WS73_CUSTOMIZE_SIZE - 124))
		return -EOPNOTSUPP;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_validate, "WS73");
