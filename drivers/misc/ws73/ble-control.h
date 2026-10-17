/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BLE_CONTROL_H
#define _WS73_BLE_CONTROL_H

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#include "service.h"

enum ws73_ble_control_state {
	WS73_BLE_UNKNOWN,
	WS73_BLE_WAITING,
	WS73_BLE_REPLIED,
	WS73_BLE_ENABLED,
	WS73_BLE_DISABLED,
	WS73_BLE_FAILED,
};

struct ws73_ble_control {
	struct mutex command_lock;
	spinlock_t lock;
	struct completion done;
	enum ws73_ble_control_state state;
	u64 deadline;
	bool target;
	int error;
};

struct ws73_ble_control_io {
	void *context;
	u64 (*now_ms)(void *context);
	bool (*cancelled)(void *context);
	/*
	 * Synchronous USB class/interface OUT, request/value/index = 0,
	 * wLength = 4. Copy the borrowed command into DMA-safe storage.
	 * Return actual data bytes or errno. The backend must serialize
	 * admission with disconnect and support cancellation/draining.
	 */
	int (*send)(void *context, const u8 command[4], unsigned int timeout_ms);
};

/* Fresh object only. No reset API; a failed instance cannot be reused. */
void ws73_ble_control_init(struct ws73_ble_control *control);
/* Non-sleeping RX/abort, independent of the caller waiting for completion. */
int ws73_ble_control_receive(struct ws73_ble_control *control,
			     enum ws73_notification notification, u64 now_ms);
void ws73_ble_control_abort(struct ws73_ble_control *control, int error);
/*
 * Sleepable action transaction. Initial state is unknown: only enable may
 * start it. Thereafter successful opposite actions can alternate. Repeating
 * a stable state returns -EALREADY; concurrent admission returns -EBUSY.
 * Both actual four-byte TX and matching action reply must succeed within
 * one deadline, capped at 2000 ms total. Any failure is terminal. Owner must
 * abort, stop/join RX and join this caller before freeing state.
 *
 * Caller must first establish firmware/board/customization prerequisites.
 * Success confirms only the action, not BOOT_FINISH, HCI setup or readiness.
 * Tokenless replies cannot authenticate an old device epoch; initialization
 * must establish that epoch before this operation can authorize a consumer.
 */
int ws73_ble_control_set(struct ws73_ble_control *control,
			 const struct ws73_ble_control_io *io, bool enabled,
			 u64 deadline_ms);

#endif
