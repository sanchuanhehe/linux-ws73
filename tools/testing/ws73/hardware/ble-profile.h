/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_BLE_TRIAL_PROFILE_H
#define WS73_BLE_TRIAL_PROFILE_H

#include <crypto/sha2.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <linux/ws73.h>
#include "customize.h"

#define WS73_BLE_TRIAL_PROFILE "ws73-trial/customization-body.bin"

/* Observed separately on U and E; this is a trial gate, not board provisioning. */
static inline bool ws73_ble_trial_identity(int expected,
					   const struct btws73_chip_identity *chip,
					   const struct ws73_version *version)
{
	return (expected == 0 || expected == 2) && chip && version &&
	       chip->package == expected && chip->bus == 0 &&
	       version->firmware == 0x010a6e3c && version->rom == 0x00010101;
}

/* One private SDK110 INI candidate, not a public or automatically selected board. */
static inline int ws73_ble_trial_profile(const u8 *data, size_t length,
					 struct ws73_customization *out)
{
	static const u8 expected[SHA256_DIGEST_SIZE] = {
		0x61, 0x5e, 0x2e, 0xe9, 0x21, 0xfb, 0xb5, 0xa0,
		0x51, 0x9c, 0xb7, 0x63, 0x61, 0x70, 0x8f, 0x68,
		0x47, 0x4f, 0x10, 0xb2, 0x1c, 0x0d, 0x18, 0x14,
		0x0b, 0x7c, 0x18, 0xf9, 0xcf, 0x76, 0x31, 0x6c,
	};
	struct ws73_customization p = {};
	u8 digest[SHA256_DIGEST_SIZE], record[WS73_CUSTOMIZE_SIZE];
	unsigned int i;
	int ret;

	if (!data || !out)
		return -EINVAL;
	if (length != WS73_CUSTOMIZE_BODY_SIZE)
		return -EMSGSIZE;
	sha256(data, length, digest);
	if (memcmp(digest, expected, sizeof(digest)))
		return -EBADMSG;
	p.coexistence = get_unaligned_le32(data);
	p.disable_ll_privacy = get_unaligned_le32(data + 4);
	p.suspend_mode = get_unaligned_le32(data + 8);
	p.scan_interval = get_unaligned_le32(data + 12);
	p.scan_window = get_unaligned_le32(data + 16);
	p.front_switch = get_unaligned_le32(data + 20);
	p.max_power = get_unaligned_le32(data + 24);
	p.calibration_count = get_unaligned_le32(data + 28);
	for (i = 0; i < 8; i++) {
		p.calibration[i].reference = get_unaligned_le16(data + 32 + 4 * i);
		p.calibration[i].frequency = get_unaligned_le16(data + 34 + 4 * i);
		p.srrc[i].reference = get_unaligned_le16(data + 84 + 4 * i);
		p.srrc[i].frequency = get_unaligned_le16(data + 86 + 4 * i);
	}
	for (i = 0; i < 6; i += 2) {
		p.channel_boundary[i] = get_unaligned_le16(data + 66 + 2 * i);
		p.channel_boundary[i + 1] = get_unaligned_le16(data + 64 + 2 * i);
	}
	p.channel_boundary[6] = get_unaligned_le16(data + 76);
	p.srrc_switch = get_unaligned_le32(data + 80);
	ret = ws73_customize_encode(record, sizeof(record), &p);
	if (ret < 0)
		return ret;
	if (memcmp(record + 8, data, length))
		return -EPROTO;
	*out = p;
	return 0;
}
#endif
