/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_USB_TX_H
#define _WS73_USB_TX_H

#include <linux/mutex.h>

#include "usb-wire.h"

struct ws73_usb_tx_io {
	void *context;
	u64 (*now_ms)(void *context);
	bool (*cancelled)(void *context);
	/* Sleepable; actual byte count or errno. Must abort on disconnect. */
	int (*send)(void *context, const u8 *data, size_t length, unsigned int timeout_ms);
};

struct ws73_usb_tx {
	struct mutex lock;
	u32 sequence;
	u8 queue;
	bool queue_valid;
	int failure;
	u8 buffer[WS73_USB_TX_MAX];
};

/* Initialize once per fresh runtime instance, never to recover a failed one. */
void ws73_usb_tx_init(struct ws73_usb_tx *tx);

/*
 * Synchronous borrowed submission; no skb ownership transfer or software
 * queue. Serializes control/data for this device, with one fixed scratch
 * buffer and one in-flight transfer. No callback may re-enter this object.
 * Success means local transfer completion, not a device reply or credit.
 * Any transport error, cancellation or deadline expiry makes it terminal.
 * Owner must stop new callers, cancel blocked I/O and join all callers before
 * freeing tx or io. An old instance must never be reused after re-enumeration.
 */
int ws73_usb_tx_record(struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			const u8 *record, size_t length, u64 deadline_ms);

#endif
