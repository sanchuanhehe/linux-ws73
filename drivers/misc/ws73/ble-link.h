/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BLE_LINK_H
#define _WS73_BLE_LINK_H

#include "ble.h"
#include "startup.h"

struct ws73_ble_link {
	spinlock_t lock; /* Child publication, receive/fault routing and detach. */
	struct auxiliary_device *child;
	struct ws73_startup *startup;
	struct ws73_usb_tx *tx;
	struct ws73_usb_tx_io bulk;
	struct ws73_ble_control_io control;
	void (*transport_fault)(void *context, int error);
	void *fault_context;
	int error;
};

void ws73_ble_link_init(struct ws73_ble_link *link, struct ws73_startup *startup,
			struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *bulk,
			const struct ws73_ble_control_io *control,
			void (*transport_fault)(void *, int), void *context);
/* One owner serializes publish/remove, joins producers before remove, and
 * keeps all referenced objects alive until removal returns. No lock needed
 * by RX may be held while publishing: probing can start the consumer.
 * Firmware/identity/board/address verification remains the owner's obligation.
 * Startup must be DONE, BLE disabled with acknowledgment and old data drained
 * by the owner. Neither these state checks nor a successful add prove an epoch.
 */
int ws73_ble_link_publish(struct ws73_ble_link *link, struct device *parent);
void ws73_ble_link_remove(struct ws73_ble_link *link);
void ws73_ble_link_abort(struct ws73_ble_link *link, int error);
/* Validated queue-7 HCC records; copies through the actual consumer callback. */
void ws73_ble_link_receive(struct ws73_ble_link *link, const struct ws73_usb_rx *rx);
int ws73_ble_link_notify(struct ws73_ble_link *link, enum ws73_notification event, u64 now);

#endif
