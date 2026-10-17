/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_SERVICE_H
#define _WS73_SERVICE_H

#include <linux/types.h>
#include <linux/bits.h>

#define WS73_INTERRUPT_SIZE 8
#define WS73_INTERRUPT_BSP_READY BIT(0)
#define WS73_INTERRUPT_PACKET_LOSS BIT(5)
#define WS73_INTERRUPT_PANIC (BIT(6) | BIT(18))

struct ws73_interrupt {
	u32 events;
	u32 high_priority_pool;
};

/* Raw little-endian words, exact length; neither field authorizes a send. */
int ws73_interrupt_decode(const u8 *data, size_t length, struct ws73_interrupt *result);

enum ws73_notification {
	WS73_NOTIFY_BOOT,
	WS73_NOTIFY_CUSTOMIZED,
	WS73_NOTIFY_BLE_OPEN,
	WS73_NOTIFY_BLE_CLOSE,
};

/*
 * Decode SDK110 service 5, subtype 0 (byte 0 = 0x50), queue 10 notifications.
 * The RX HCC length excludes its header; the tag length excludes both
 * HCC and tag headers. Do not apply the SDK Host TX allocation-size quirk.
 * Input may include transport padding. No borrowed storage escapes.
 * Unsupported tags/SLE actions return -EOPNOTSUPP; malformed known messages
 * return another errno. Output is unchanged on any error. A notification
 * alone is not proof of an outstanding request's completion or readiness.
 */
int ws73_notification_decode(const u8 *record, size_t length,
			     enum ws73_notification *notification);

#endif
