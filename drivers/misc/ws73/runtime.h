/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_RUNTIME_H
#define _WS73_RUNTIME_H

#include <linux/completion.h>
#include <linux/spinlock.h>

#include "usb-tx.h"

enum ws73_identity_state {
	WS73_IDENTITY_UNUSED,
	WS73_IDENTITY_WAITING,
	WS73_IDENTITY_REPLIED,
	WS73_IDENTITY_DONE,
	WS73_IDENTITY_FAILED,
};

struct ws73_identity_request {
	spinlock_t lock;
	struct completion done;
	enum ws73_identity_state state;
	u64 deadline;
	int error;
	struct btws73_chip_identity identity;
};

/* Initialize once per runtime instance. No reset or retry after use. */
void ws73_identity_init(struct ws73_identity_request *request);
/* Atomic-context-safe RX/abort entry points, independent of the TX waiter. */
int ws73_identity_receive(struct ws73_identity_request *request,
			  const u8 *record, size_t length, u64 now_ms);
void ws73_identity_abort(struct ws73_identity_request *request, int error);
/*
 * Sleepable one-shot GET_CHIP_TYPE. Output unchanged on error. The caller
 * must start RX first, using a completion path independent of this waiter.
 * A reply alone cannot override failed TX, disconnect, or the deadline.
 * Owner must abort, stop/join RX and join this caller before freeing state.
 * Success is runtime identity only, not a boot/runtime compatibility check.
 */
int ws73_identity_query(struct ws73_identity_request *request,
			struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			u64 deadline_ms, struct btws73_chip_identity *identity);

#endif
