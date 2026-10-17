/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_SERVICE_H
#define _WS73_SERVICE_H

#include <linux/types.h>

enum ws73_notification {
	WS73_NOTIFY_BOOT,
	WS73_NOTIFY_CUSTOMIZED,
	WS73_NOTIFY_BLE_OPEN,
	WS73_NOTIFY_BLE_CLOSE,
};

/*
 * Decode SDK110 service 5, subtype 0 (byte 0 = 0x50), queue 10 notifications.
 * The HCC
 * length includes its header; the tag length excludes HCC and tag headers.
 * Input may include transport padding. No borrowed storage escapes.
 * Unsupported tags/SLE actions return -EOPNOTSUPP; malformed known messages
 * return another errno. Output is unchanged on any error. A notification
 * alone is not proof of an outstanding request's completion or readiness.
 */
int ws73_notification_decode(const u8 *record, size_t length,
			     enum ws73_notification *notification);

#endif
