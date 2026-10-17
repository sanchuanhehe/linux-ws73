/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_STARTUP_H
#define _WS73_STARTUP_H

#include "ble-control.h"
#include "platform-config.h"
#include "customize-request.h"

enum ws73_startup_state {
	WS73_STARTUP_UNUSED,
	WS73_STARTUP_ARMED,
	WS73_STARTUP_WAIT_BSP,
	WS73_STARTUP_CUSTOMIZE,
	WS73_STARTUP_ENABLE,
	WS73_STARTUP_WAIT_BOOT,
	WS73_STARTUP_DONE,
	WS73_STARTUP_FAILED,
	WS73_STARTUP_CONFIGURE,
};

struct ws73_startup {
	struct mutex command_lock; /* One sleepable startup caller. */
	spinlock_t lock; /* Stage and latches shared with receive/abort. */
	struct completion bsp_done;
	struct completion boot_done;
	struct ws73_customize_request customization;
	struct ws73_ble_control ble;
	enum ws73_startup_state state;
	u64 deadline;
	int error;
	bool bsp_seen;
	bool boot_seen;
};

void ws73_startup_init(struct ws73_startup *startup);
/* Arm once before enabling RX; captures observations, not firmware trust. */
int ws73_startup_arm(struct ws73_startup *startup, u64 now_ms, u64 deadline_ms);
void ws73_startup_abort(struct ws73_startup *startup, int error);
/* Atomic-safe; IRQ faults override BSP, unrelated events cannot satisfy it. */
int ws73_startup_interrupt(struct ws73_startup *startup,
			   const struct ws73_interrupt *event, u64 now_ms);
int ws73_startup_receive(struct ws73_startup *startup,
			 enum ws73_notification notification, u64 now_ms);
/*
 * One-shot BSP -> platform configuration -> customization -> BLE enable -> BOOT handshakes after arm.
 * A current BOOT can precede its wait, including while identity is checked.
 * Caller must establish firmware epoch, identity/version and board validity.
 * All clocks must use the same monotonic millisecond domain. No locks may be
 * held that RX needs. The owner must abort and join this caller/RX on removal.
 * DONE is not HCI readiness: address setup and HCI registration remain separate.
 * Failure leaves the function uncertain; no automatic disable/reset/retry.
 */
int ws73_startup_run(struct ws73_startup *startup, struct ws73_usb_tx *tx,
		     const struct ws73_usb_tx_io *bulk,
		     const struct ws73_ble_control_io *control,
		     const struct ws73_platform_config *platform,
		     const struct ws73_customization *parameters);

#endif
