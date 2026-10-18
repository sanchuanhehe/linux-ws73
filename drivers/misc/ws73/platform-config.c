// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "platform-config.h"

int ws73_platform_config_encode(u8 *out, size_t capacity,
				const struct ws73_platform_config *config)
{
	u32 value;

	if (!out || !config)
		return -EINVAL;
	if (config->wake_gpio > U8_MAX)
		return -ERANGE;
	if (capacity < 32)
		return -ENOSPC;
	value = config->wake_gpio << 1 | config->wake_high << 9 |
		config->usb_link_check << 10;
	memset(out, 0, 32);
	out[0] = 0x19;
	out[1] = 9;
	put_unaligned_le16(32, out + 2);
	/* Platform request 0, four-byte PM configuration. Bit 0 stays zero. */
	put_unaligned_le16(4, out + 6);
	put_unaligned_le32(value, out + 8);
	return 32;
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_config_encode, "WS73");

int ws73_platform_configure(struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			    const struct ws73_platform_config *config, u64 deadline)
{
	u8 record[32];
	int ret;

	ret = ws73_platform_config_encode(record, sizeof(record), config);
	if (ret < 0)
		return ret;
	return ws73_usb_tx_record(tx, io, record, sizeof(record), deadline);
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_configure, "WS73");
