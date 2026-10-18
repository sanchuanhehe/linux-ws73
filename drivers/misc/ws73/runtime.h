/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_RUNTIME_H
#define _WS73_RUNTIME_H

#include <linux/completion.h>
#include <linux/spinlock.h>

#include "usb-tx.h"

enum ws73_platform_state {
	WS73_PLATFORM_UNUSED,
	WS73_PLATFORM_WAITING,
	WS73_PLATFORM_REPLIED,
	WS73_PLATFORM_DONE,
	WS73_PLATFORM_FAILED,
};

union ws73_platform_result {
	struct btws73_chip_identity chip;
	struct ws73_version version;
};

struct ws73_platform_request {
	/* Serializes the sleepable query transaction. */
	struct mutex command_lock;
	/* Protects request state shared with RX and abort callbacks. */
	spinlock_t lock;
	struct completion done;
	enum ws73_platform_state state;
	u64 deadline;
	int error;
	u8 used;
	u16 expected_reply;
	union ws73_platform_result result;
};

/* Initialize once per runtime instance. No reset or retry after use. */
void ws73_platform_init(struct ws73_platform_request *request);
/* Atomic-context-safe RX/abort entry points, independent of the TX waiter. */
int ws73_platform_receive(struct ws73_platform_request *request,
			  const u8 *record, size_t length, u64 now_ms);
void ws73_platform_abort(struct ws73_platform_request *request, int error);
/*
 * Sleepable GET_CHIP_TYPE / GET_VERSION, each once per instance, serialized.
 * A concurrent caller gets -EBUSY; repeating a consumed kind gets -EALREADY.
 * Output unchanged on error. The caller
 * must start RX first, using a completion path independent of this waiter.
 * A reply alone cannot override failed TX, disconnect, or the deadline.
 * Owner must abort, stop/join RX and join this caller before freeing state.
 * Success reports wire values, not a boot/runtime compatibility check.
 */
int ws73_platform_get_chip(struct ws73_platform_request *request,
			   struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			   u64 deadline_ms, struct btws73_chip_identity *identity);
int ws73_platform_get_version(struct ws73_platform_request *request,
			      struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			      u64 deadline_ms, struct ws73_version *version);

#endif
