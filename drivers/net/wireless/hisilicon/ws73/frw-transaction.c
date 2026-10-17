// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/string.h>

#include "frw-transaction.h"

void ws73_frw_transaction_init(struct ws73_frw_transaction *transaction)
{
	mutex_init(&transaction->command_lock);
	spin_lock_init(&transaction->lock);
	init_completion(&transaction->done);
	transaction->state = WS73_FRW_IDLE;
	transaction->next_sequence = 1;
	transaction->error = 0;
}

static void ws73_frw_fail_locked(struct ws73_frw_transaction *transaction,
				 int error)
{
	if (transaction->state == WS73_FRW_FAILED)
		return;
	transaction->error = error < 0 ? error : -ESHUTDOWN;
	transaction->state = WS73_FRW_FAILED;
	complete_all(&transaction->done);
}

void ws73_frw_transaction_abort(struct ws73_frw_transaction *transaction,
				int error)
{
	unsigned long flags;

	spin_lock_irqsave(&transaction->lock, flags);
	ws73_frw_fail_locked(transaction, error);
	spin_unlock_irqrestore(&transaction->lock, flags);
}

void ws73_frw_transaction_shutdown(struct ws73_frw_transaction *transaction)
{
	ws73_frw_transaction_abort(transaction, -ESHUTDOWN);
	mutex_lock(&transaction->command_lock);
	mutex_unlock(&transaction->command_lock);
}

int ws73_frw_transaction_receive(struct ws73_frw_transaction *transaction,
				 const u8 *record, size_t length)
{
	struct ws73_frw_reply reply;
	unsigned long flags;
	int ret;

	if (!transaction || !record)
		return -EINVAL;
	spin_lock_irqsave(&transaction->lock, flags);
	if (transaction->state == WS73_FRW_FAILED) {
		ret = transaction->error;
		goto unlock;
	}
	if (transaction->state == WS73_FRW_IDLE) {
		ret = -EALREADY;
		goto unlock;
	}
	ret = ws73_frw_reply_decode(record, length, &transaction->key,
				    transaction->capacity, &reply);
	if (ret == -ENOENT)
		goto unlock;
	if (ktime_get_ns() >= transaction->deadline_ns)
		ret = -ETIMEDOUT;
	else if (!ret && transaction->state == WS73_FRW_REPLIED)
		ret = -EPROTO;
	if (ret) {
		ws73_frw_fail_locked(transaction, ret);
		goto unlock;
	}
	transaction->reply.device_status = reply.device_status;
	transaction->reply.length = reply.length;
	memcpy(transaction->reply.data, reply.data, reply.length);
	transaction->state = WS73_FRW_REPLIED;
	complete_all(&transaction->done);
unlock:
	spin_unlock_irqrestore(&transaction->lock, flags);
	return ret;
}

int ws73_frw_transact(struct ws73_frw_transaction *transaction,
		      const struct ws73_frw_transport *transport, u16 id,
		      u8 vap, const u8 *data, size_t length,
		      size_t reply_capacity, unsigned int timeout_ms,
		      struct ws73_frw_result *result)
{
	unsigned long flags, remaining;
	u64 now, deadline;
	int size, ret;

	if (!transaction || !transport || !transport->send || !result ||
	    (!data && length) || vap > 15 || !timeout_ms ||
	    timeout_ms > WS73_FRW_TIMEOUT_MAX_MS)
		return -EINVAL;
	if (length > WS73_FRW_DATA_MAX || reply_capacity > WS73_FRW_DATA_MAX)
		return -EMSGSIZE;
	if (!mutex_trylock(&transaction->command_lock))
		return -EBUSY;
	spin_lock_irqsave(&transaction->lock, flags);
	if (transaction->state == WS73_FRW_FAILED) {
		ret = transaction->error;
		goto unlock_state;
	}
	if (transaction->next_sequence > U32_MAX) {
		ret = -EOVERFLOW;
		ws73_frw_fail_locked(transaction, ret);
		goto unlock_state;
	}
	transaction->key.id = id;
	transaction->key.vap = vap;
	transaction->key.sequence = transaction->next_sequence++;
	deadline = ktime_get_ns() + (u64)timeout_ms * NSEC_PER_MSEC;
	transaction->deadline_ns = deadline;
	transaction->capacity = reply_capacity;
	/* Clear unused bytes before copying the complete owned result to caller. */
	memset(&transaction->reply, 0, sizeof(transaction->reply));
	reinit_completion(&transaction->done);
	transaction->state = WS73_FRW_WAITING;
	spin_unlock_irqrestore(&transaction->lock, flags);

	size = ws73_frw_request_encode(transaction->command,
				       sizeof(transaction->command),
				       &transaction->key, data, length,
				       reply_capacity);
	if (size < 0) {
		ret = size;
		goto failed;
	}
	/* send must enforce the deadline as well; scheduling can consume it. */
	if (ktime_get_ns() >= deadline) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	ret = transport->send(transport->context, transaction->command, size,
			      deadline);
	if (ret != size) {
		if (ret >= 0)
			ret = -EIO;
		goto failed;
	}
	now = ktime_get_ns();
	remaining = now < deadline ? nsecs_to_jiffies(deadline - now) + 1 : 0;
	if (!remaining || !wait_for_completion_timeout(&transaction->done, remaining)) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	spin_lock_irqsave(&transaction->lock, flags);
	if (transaction->state == WS73_FRW_FAILED) {
		ret = transaction->error;
	} else if (ktime_get_ns() >= deadline) {
		ret = -ETIMEDOUT;
		ws73_frw_fail_locked(transaction, ret);
	} else if (transaction->state == WS73_FRW_REPLIED) {
		*result = transaction->reply;
		transaction->state = WS73_FRW_IDLE;
		ret = 0;
	} else {
		ret = -EPROTO;
		ws73_frw_fail_locked(transaction, ret);
	}
	goto unlock_state;
failed:
	spin_lock_irqsave(&transaction->lock, flags);
	ws73_frw_fail_locked(transaction, ret);
	ret = transaction->error;
unlock_state:
	spin_unlock_irqrestore(&transaction->lock, flags);
	mutex_unlock(&transaction->command_lock);
	return ret;
}
