/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_CUSTOMIZE_REQUEST_H
#define _WS73_CUSTOMIZE_REQUEST_H

#include <linux/completion.h>
#include <linux/spinlock.h>

#include "customize.h"
#include "usb-tx.h"

enum ws73_customize_state {
	WS73_CUSTOM_UNUSED,
	WS73_CUSTOM_PREPARING,
	WS73_CUSTOM_WAITING,
	WS73_CUSTOM_REPLIED,
	WS73_CUSTOM_DONE,
	WS73_CUSTOM_FAILED,
};

struct ws73_customize_request {
	struct mutex command_lock; /* One sleepable apply caller. */
	spinlock_t lock; /* State shared with RX and abort. */
	struct completion done;
	enum ws73_customize_state state;
	u64 deadline;
	int error;
};

/* Fresh device epoch only: there is deliberately no reset/retry API. */
void ws73_customize_request_init(struct ws73_customize_request *request);
/* Non-sleeping, independent RX/abort paths. */
int ws73_customize_receive(struct ws73_customize_request *request,
			   enum ws73_notification notification, u64 now_ms);
void ws73_customize_abort(struct ws73_customize_request *request, int error);
/*
 * Sleepable, one-shot transaction. Caller supplies board-qualified parameters
 * and a known firmware epoch. Zero means USB TX and CUSTOMIZE_RECEIVED passed,
 * not BOOT_FINISH, BLE_OPEN, HCI readiness or RF qualification. One absolute
 * deadline (capped at 2 seconds) covers queue controls, data and the reply.
 * Owner must abort and join this caller and RX before freeing the object.
 */
int ws73_customize_apply(struct ws73_customize_request *request,
			 struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			 const struct ws73_customization *parameters, u64 deadline_ms);

#endif
