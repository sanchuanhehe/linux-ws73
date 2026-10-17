// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "runtime.h"

struct ws73_identity_io {
	struct ws73_identity_request *request;
	const struct ws73_usb_tx_io *transport;
};

static u64 ws73_identity_now(void *context)
{
	struct ws73_identity_io *io = context;

	return io->transport->now_ms(io->transport->context);
}

static bool ws73_identity_cancelled(void *context)
{
	struct ws73_identity_io *io = context;
	unsigned long flags;
	bool failed;

	spin_lock_irqsave(&io->request->lock, flags);
	failed = io->request->state == WS73_IDENTITY_FAILED;
	spin_unlock_irqrestore(&io->request->lock, flags);
	return failed || io->transport->cancelled(io->transport->context);
}

static int ws73_identity_send(void *context, const u8 *data, size_t length,
			      unsigned int timeout)
{
	struct ws73_identity_io *io = context;

	return io->transport->send(io->transport->context, data, length, timeout);
}

void ws73_identity_init(struct ws73_identity_request *request)
{
	spin_lock_init(&request->lock);
	init_completion(&request->done);
	request->state = WS73_IDENTITY_UNUSED;
	request->error = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_identity_init, "WS73");

static void ws73_identity_fail_locked(struct ws73_identity_request *request, int error)
{
	if (request->state == WS73_IDENTITY_FAILED)
		return;
	request->error = error < 0 ? error : -ESHUTDOWN;
	request->state = WS73_IDENTITY_FAILED;
	complete_all(&request->done);
}

void ws73_identity_abort(struct ws73_identity_request *request, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&request->lock, flags);
	ws73_identity_fail_locked(request, error);
	spin_unlock_irqrestore(&request->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_identity_abort, "WS73");

int ws73_identity_receive(struct ws73_identity_request *request,
			  const u8 *record, size_t length, u64 now_ms)
{
	struct btws73_chip_identity identity;
	unsigned long flags;
	int ret;

	if (!request || !record)
		return -EINVAL;
	/* Other platform messages cannot satisfy this request. */
	if (length < 6 || record[0] != 0x19 || record[1] != 9 ||
	    get_unaligned_le16(record + 4) != 10)
		return -ENOENT;
	ret = btws73_chip_reply_decode(record, length, &identity);
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_IDENTITY_FAILED) {
		ret = request->error;
	} else if (request->state == WS73_IDENTITY_REPLIED) {
		/* More than one matching reply before consumption is ambiguous. */
		ret = -EPROTO;
		ws73_identity_fail_locked(request, ret);
	} else if (request->state != WS73_IDENTITY_WAITING) {
		ret = -EALREADY;
	} else if (now_ms >= request->deadline) {
		ret = -ETIMEDOUT;
		ws73_identity_fail_locked(request, ret);
	} else if (ret) {
		ws73_identity_fail_locked(request, ret);
	} else {
		request->identity = identity;
		request->state = WS73_IDENTITY_REPLIED;
		complete_all(&request->done);
	}
	spin_unlock_irqrestore(&request->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_identity_receive, "WS73");

int ws73_identity_query(struct ws73_identity_request *request,
			struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			u64 deadline_ms, struct btws73_chip_identity *identity)
{
	struct ws73_identity_io context = { request, io };
	const struct ws73_usb_tx_io guarded_io = {
		.context = &context,
		.now_ms = ws73_identity_now,
		.cancelled = ws73_identity_cancelled,
		.send = ws73_identity_send,
	};
	unsigned long flags;
	u8 command[32];
	u64 now;
	int ret;

	if (!request || !tx || !io || !io->now_ms || !io->cancelled ||
	    !io->send || !identity)
		return -EINVAL;
	spin_lock_irqsave(&request->lock, flags);
	if (request->state != WS73_IDENTITY_UNUSED) {
		ret = request->state == WS73_IDENTITY_FAILED ? request->error : -EALREADY;
		spin_unlock_irqrestore(&request->lock, flags);
		return ret;
	}
	request->deadline = deadline_ms;
	request->state = WS73_IDENTITY_WAITING;
	spin_unlock_irqrestore(&request->lock, flags);

	btws73_chip_query_encode(command, sizeof(command));
	ret = ws73_usb_tx_record(tx, &guarded_io, command, sizeof(command), deadline_ms);
	if (ret)
		goto failed;
	now = io->now_ms(io->context);
	if (now >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	/* Bound reply waiting even if a caller supplies a distant deadline. */
	if (!wait_for_completion_timeout(&request->done,
		msecs_to_jiffies(min_t(u64, deadline_ms - now, 2000)))) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	if (io->cancelled(io->context)) {
		ret = -ENODEV;
		goto failed;
	}
	if (io->now_ms(io->context) >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_IDENTITY_REPLIED) {
		*identity = request->identity;
		request->state = WS73_IDENTITY_DONE;
		ret = 0;
	} else {
		ret = request->state == WS73_IDENTITY_FAILED ? request->error : -EPROTO;
	}
	spin_unlock_irqrestore(&request->lock, flags);
	return ret;
failed:
	ws73_identity_abort(request, ret);
	/* Preserve the first terminal error, including a concurrent disconnect. */
	spin_lock_irqsave(&request->lock, flags);
	ret = request->error;
	spin_unlock_irqrestore(&request->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_identity_query, "WS73");
