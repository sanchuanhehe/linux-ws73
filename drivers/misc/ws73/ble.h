/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BLE_H
#define _WS73_BLE_H

#include <linux/ws73-ble.h>

struct ws73_ble_backend {
	int (*set_enabled)(void *context, bool enabled);
	int (*send)(void *context, const u8 *record, size_t length);
};

/*
 * Publish only after verified USB U/E identity, firmware epoch/version,
 * board/address inputs and startup handshakes, with BLE disabled and its old
 * data drained. No public qualification knob.
 * Backend operations sleep, have finite deadlines and support owner abort.
 * They must not call remove. The owner must abort IO and join all receive/
 * fault producers before remove, which consumes its returned device reference.
 * Backend storage/context remain valid until remove returns; no callback is
 * retained after that, even if an external device reference delays release.
 * Registration can synchronously probe the actual HCI consumer.
 */
struct auxiliary_device *ws73_ble_add(struct device *parent,
				      const struct ws73_ble_backend *backend, void *context);
void ws73_ble_remove(struct auxiliary_device *adev);
/* Internal two-phase publication: install RX routing before synchronous probe.
 * On auxiliary_device_add() failure, detach/drain routing then discard.
 */
struct auxiliary_device *ws73_ble_prepare(struct device *parent,
					  const struct ws73_ble_backend *backend, void *context);
void ws73_ble_discard(struct auxiliary_device *adev);
/* Borrowed validated H4 fragment. No URB memory survives this callback. */
void ws73_ble_receive(struct auxiliary_device *adev, const u8 *data, size_t length);

#endif
