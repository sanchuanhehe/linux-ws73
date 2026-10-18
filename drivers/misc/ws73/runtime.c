// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "runtime.h"

struct ws73_platform_io {
	struct ws73_platform_request *request;
	const struct ws73_usb_tx_io *transport;
};

static u64 ws73_platform_now(void *context)
{
	struct ws73_platform_io *io = context;

	return io->transport->now_ms(io->transport->context);
}

static bool ws73_platform_cancelled(void *context)
{
	struct ws73_platform_io *io = context;
	unsigned long flags;
	bool failed;

	spin_lock_irqsave(&io->request->lock, flags);
	failed = io->request->state == WS73_PLATFORM_FAILED;
	spin_unlock_irqrestore(&io->request->lock, flags);
	return failed || io->transport->cancelled(io->transport->context);
}

static int ws73_platform_send(void *context, const u8 *data, size_t length,
			      unsigned int timeout)
{
	struct ws73_platform_io *io = context;

	return io->transport->send(io->transport->context, data, length, timeout);
}

void ws73_platform_init(struct ws73_platform_request *request)
{
	mutex_init(&request->command_lock);
	spin_lock_init(&request->lock);
	init_completion(&request->done);
	request->state = WS73_PLATFORM_UNUSED;
	request->error = 0;
	request->used = 0;
	request->expected_reply = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_init, "WS73");

static void ws73_platform_fail_locked(struct ws73_platform_request *request, int error)
{
	if (request->state == WS73_PLATFORM_FAILED)
		return;
	request->error = error < 0 ? error : -ESHUTDOWN;
	request->state = WS73_PLATFORM_FAILED;
	complete_all(&request->done);
}

void ws73_platform_abort(struct ws73_platform_request *request, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&request->lock, flags);
	ws73_platform_fail_locked(request, error);
	spin_unlock_irqrestore(&request->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_abort, "WS73");

int ws73_platform_receive(struct ws73_platform_request *request,
			  const u8 *record, size_t length, u64 now_ms)
{
	union ws73_platform_result result = {};
	unsigned long flags;
	u16 reply;
	int ret;

	if (!request || !record)
		return -EINVAL;
	/* Other platform messages cannot satisfy this request. */
	if (length < 6 || record[0] != 0x19 || record[1] != 9)
		return -ENOENT;
	reply = get_unaligned_le16(record + 4);
	if (reply == 10)
		ret = btws73_chip_reply_decode(record, length, &result.chip);
	else if (reply == 7)
		ret = ws73_version_reply_decode(record, length, &result.version);
	else
		return -ENOENT;
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_PLATFORM_FAILED) {
		ret = request->error;
	} else if (request->state == WS73_PLATFORM_UNUSED) {
		ret = -EALREADY;
	} else if (reply != request->expected_reply) {
		ret = -ENOENT;
	} else if (request->state == WS73_PLATFORM_REPLIED) {
		/* More than one matching reply before consumption is ambiguous. */
		ret = -EPROTO;
		ws73_platform_fail_locked(request, ret);
	} else if (request->state != WS73_PLATFORM_WAITING) {
		ret = -EALREADY;
	} else if (now_ms >= request->deadline) {
		ret = -ETIMEDOUT;
		ws73_platform_fail_locked(request, ret);
	} else if (ret) {
		ws73_platform_fail_locked(request, ret);
	} else {
		request->result = result;
		request->state = WS73_PLATFORM_REPLIED;
		complete_all(&request->done);
	}
	spin_unlock_irqrestore(&request->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_receive, "WS73");

static int ws73_platform_query(struct ws73_platform_request *request,
			       struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			       u64 deadline_ms, bool version, union ws73_platform_result *result)
{
	struct ws73_platform_io context = { request, io };
	const struct ws73_usb_tx_io guarded_io = {
		.context = &context,
		.now_ms = ws73_platform_now,
		.cancelled = ws73_platform_cancelled,
		.send = ws73_platform_send,
	};
	unsigned long flags;
	u8 command[32];
	u8 kind = version ? 2 : 1;
	u64 now;
	int ret;

	if (!request || !tx || !io || !io->now_ms || !io->cancelled ||
	    !io->send || !result)
		return -EINVAL;
	if (!mutex_trylock(&request->command_lock))
		return -EBUSY;
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_PLATFORM_FAILED || (request->used & kind)) {
		ret = request->state == WS73_PLATFORM_FAILED ? request->error : -EALREADY;
		spin_unlock_irqrestore(&request->lock, flags);
		goto unlock_command;
	}
	request->used |= kind;
	request->expected_reply = version ? 7 : 10;
	reinit_completion(&request->done);
	request->deadline = deadline_ms;
	request->state = WS73_PLATFORM_WAITING;
	spin_unlock_irqrestore(&request->lock, flags);

	if (version)
		ws73_version_query_encode(command, sizeof(command));
	else
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
	if (request->state == WS73_PLATFORM_REPLIED) {
		*result = request->result;
		request->state = WS73_PLATFORM_DONE;
		ret = 0;
	} else {
		ret = request->state == WS73_PLATFORM_FAILED ? request->error : -EPROTO;
	}
	spin_unlock_irqrestore(&request->lock, flags);
	goto unlock_command;
failed:
	ws73_platform_abort(request, ret);
	/* Preserve the first terminal error, including a concurrent disconnect. */
	spin_lock_irqsave(&request->lock, flags);
	ret = request->error;
	spin_unlock_irqrestore(&request->lock, flags);
unlock_command:
	mutex_unlock(&request->command_lock);
	return ret;
}

int ws73_platform_get_chip(struct ws73_platform_request *request,
			   struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			   u64 deadline_ms, struct btws73_chip_identity *identity)
{
	union ws73_platform_result result;
	int ret;

	if (!identity)
		return -EINVAL;
	ret = ws73_platform_query(request, tx, io, deadline_ms, false, &result);
	if (!ret)
		*identity = result.chip;
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_get_chip, "WS73");

int ws73_platform_get_version(struct ws73_platform_request *request,
			      struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			      u64 deadline_ms, struct ws73_version *version)
{
	union ws73_platform_result result;
	int ret;

	if (!version)
		return -EINVAL;
	ret = ws73_platform_query(request, tx, io, deadline_ms, true, &result);
	if (!ret)
		*version = result.version;
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_platform_get_version, "WS73");
