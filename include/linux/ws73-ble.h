/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_WS73_BLE_H
#define _LINUX_WS73_BLE_H

#include <linux/auxiliary_bus.h>
#include <linux/ws73.h>

/* Callbacks run serialized under an IRQ-safe lock; do not sleep or reenter. */
struct ws73_ble_client {
	void (*receive)(void *context, const u8 *h4, size_t length);
	void (*fault)(void *context, int error);
};

/* One client per device. Unbind drains both callbacks before returning. */
int ws73_ble_bind(struct auxiliary_device *adev, const struct ws73_ble_client *client,
		  void *context);
void ws73_ble_unbind(struct auxiliary_device *adev);
/* Sleepable, serialized operations; real backend acknowledgements required. */
int ws73_ble_start(struct auxiliary_device *adev);
int ws73_ble_stop(struct auxiliary_device *adev);
/* Borrowed HCC bytes until return. Success is local TX, not HCI completion. */
int ws73_ble_send(struct auxiliary_device *adev, const u8 *record, size_t length);
/* Atomic-safe, terminal BLE-local failure; never resets another function. */
void ws73_ble_fault(struct auxiliary_device *adev, int error);

#endif
