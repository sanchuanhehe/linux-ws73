/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_FIRMWARE_H
#define _WS73_FIRMWARE_H

#include <linux/types.h>
#include <linux/ws73.h>

struct device;
struct firmware;

#define WS73_FW_COUNT 3
#define WS73_FW_HASH_HEADER 64
#define WS73_FW_MAX_SIZE (200 * 1024)

struct ws73_fw_info {
	const char *name;
	size_t size;
	u32 address;
	const char *sha256;
};

struct ws73_fw_profile {
	u8 package;
	struct ws73_fw_info image[WS73_FW_COUNT];
};

/* Views borrow immutable files; release only after all transfers drain. */
struct ws73_fw_view {
	const u8 *data;
	size_t size;
	u32 address;
};

struct ws73_firmware {
	const struct ws73_fw_profile *profile;
	const struct firmware *file[WS73_FW_COUNT];
	struct ws73_fw_view image[WS73_FW_COUNT];
};

const struct ws73_fw_profile *ws73_fw_profile(const struct btws73_chip_identity *chip);
int ws73_fw_validate(const struct ws73_fw_info *info, const u8 *data,
		     size_t size, struct ws73_fw_view *view);
/* chip must come from a verified live HCC identity, never a USB ID guess. */
int ws73_firmware_request(struct device *dev, const struct btws73_chip_identity *chip,
			  struct ws73_firmware *set);
void ws73_firmware_release(struct ws73_firmware *set);

#endif
