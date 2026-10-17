// SPDX-License-Identifier: GPL-2.0-only
/* Independently implemented SDK110 function-control notification decoding. */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/unaligned.h>
#include <linux/ws73.h>

#include "service.h"

int ws73_notification_decode(const u8 *record, size_t length,
			     enum ws73_notification *notification)
{
	enum ws73_notification parsed;
	u16 payload, tag, size;
	u32 value;

	if (!record || !notification)
		return -EINVAL;
	if (length < 8 || length > BTWS73_RX_MAX_RECORD_SIZE)
		return -EMSGSIZE;
	if (record[0] != 0x50 || record[1] != 10)
		return -EPROTO;
	payload = get_unaligned_le16(record + 2);
	if (payload < 4 || payload > length - 4)
		return -EMSGSIZE;
	tag = get_unaligned_le16(record + 4);
	size = get_unaligned_le16(record + 6);
	if (size > payload - 4)
		return -EMSGSIZE;
	if (tag != 2 && tag != 4)
		return -EOPNOTSUPP;
	if (size != sizeof(value))
		return -EMSGSIZE;
	value = get_unaligned_le32(record + 8);
	if (tag == 2) {
		if (value == 1)
			parsed = WS73_NOTIFY_BOOT;
		else if (value == 2)
			parsed = WS73_NOTIFY_CUSTOMIZED;
		else
			return -EPROTO;
	} else {
		if (!value)
			parsed = WS73_NOTIFY_BLE_OPEN;
		else if (value == 1)
			parsed = WS73_NOTIFY_BLE_CLOSE;
		else if (value == 2 || value == 3)
			return -EOPNOTSUPP;
		else
			return -EPROTO;
	}
	*notification = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_notification_decode, "WS73");
