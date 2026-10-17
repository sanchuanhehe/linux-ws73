/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_USB_WIRE_H
#define _WS73_USB_WIRE_H

#include <linux/ws73.h>

#include "service.h"

#define WS73_USB_TX_PREFIX 64
#define WS73_USB_TX_MAX (WS73_USB_TX_PREFIX + BTWS73_RX_MAX_RECORD_SIZE)
#define WS73_USB_CONTROL_SIZE 36

enum ws73_usb_control {
	WS73_USB_RESET_AGGREGATION,
	WS73_USB_SWITCH_QUEUE,
};

struct ws73_usb_rx {
	u8 queue;
	u8 count;
	struct btws73_fragment record[BTWS73_RX_MAX_RECORDS];
	/* Only populated for queue 10, after validating the entire aggregate. */
	enum ws73_notification notification[BTWS73_RX_MAX_RECORDS];
};

/*
 * Pure TX wire helpers; no device I/O or queue state. Only BLE (7) and platform
 * (9) are admitted. Caller serializes queue switch, aggregation reset and
 * data, and supplies its per-device sequence. Buffers must not overlap.
 * Return encoded byte count, or errno without modifying output on error.
 */
int ws73_usb_control_encode(u8 *out, size_t capacity, u32 sequence,
			    enum ws73_usb_control operation, u8 queue);
int ws73_usb_single_encode(u8 *out, size_t capacity, u32 sequence,
			   const u8 *record, size_t length);

/*
 * Validates all framing and supported service lengths before publication.
 * Returned records include HCC headers/padding and borrow immutable input.
 * The consumer must still validate the service message/H4 stream; this is
 * neither an HCI completion nor a platform reply match. *rx survives errors.
 * Queue 10 additionally validates and copies each function notification;
 * unsupported tags/actions reject the whole aggregate without publication.
 */
int ws73_usb_aggregate_decode(const u8 *data, size_t length,
			      struct ws73_usb_rx *rx);

#endif
