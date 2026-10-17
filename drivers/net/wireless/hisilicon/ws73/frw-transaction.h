/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_FRW_TRANSACTION_H
#define _WS73_FRW_TRANSACTION_H

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#include "frw.h"

#define WS73_FRW_TIMEOUT_MAX_MS 2000

struct ws73_frw_result {
	s32 device_status;
	size_t length;
	u8 data[WS73_FRW_DATA_MAX];
};

enum ws73_frw_state {
	WS73_FRW_IDLE,
	WS73_FRW_WAITING,
	WS73_FRW_REPLIED,
	WS73_FRW_FAILED,
};

struct ws73_frw_transaction {
	/* Serializes admission, command storage and caller draining. */
	struct mutex command_lock;
	/* Protects state, active key, deadline, error and owned reply. */
	spinlock_t lock;
	struct completion done;
	enum ws73_frw_state state;
	struct ws73_frw_key key;
	u64 next_sequence;
	u64 deadline_ns;
	size_t capacity;
	int error;
	struct ws73_frw_result reply;
	u8 command[WS73_FRW_HEAD_LEN + WS73_FRW_DATA_MAX];
};

struct ws73_frw_transport {
	void *context;
	/* Sleepable, synchronous, bounded by the absolute monotonic deadline.
	 * Return exact FRW byte count or a negative errno. Own/copy all framing
	 * and drain any borrowed command bytes before returning, including errors.
	 * RX must run independently. No successful return proves device execution.
	 */
	int (*send)(void *context, const u8 *record, size_t length,
		    u64 deadline_ns);
};

/* Initialize once per independently established device instance. Never reset
 * an old instance to reuse sequences: this API cannot establish firmware epoch.
 */
void ws73_frw_transaction_init(struct ws73_frw_transaction *transaction);
/* Sleepable; one admitted caller, concurrent calls return -EBUSY. Result is
 * unchanged on error; on success device_status is still a separate result.
 * Inputs/result must not alias transaction storage or each other.
 * All lengths are local bounds, not device credits. Timeout/uncertain transport,
 * malformed active reply and abort permanently close this instance.
 */
int ws73_frw_transact(struct ws73_frw_transaction *transaction,
		      const struct ws73_frw_transport *transport, u16 id,
		      u8 vap, const u8 *data, size_t length,
		      size_t reply_capacity, unsigned int timeout_ms,
		      struct ws73_frw_result *result);
/* Atomic-context-safe; takes an exact unpadded reply and copies accepted bytes.
 * Unmatched tuples do not complete the active command. No input is retained.
 */
int ws73_frw_transaction_receive(struct ws73_frw_transaction *transaction,
				 const u8 *record, size_t length);
void ws73_frw_transaction_abort(struct ws73_frw_transaction *transaction,
				int error);
/* Sleepable, abort and join the admitted command. Owner must also stop/join all
 * RX and other entry points before freeing, and cancel/drain its transport.
 * Do not invoke from send/RX while the command caller is waiting for that path.
 */
void ws73_frw_transaction_shutdown(struct ws73_frw_transaction *transaction);

#endif
