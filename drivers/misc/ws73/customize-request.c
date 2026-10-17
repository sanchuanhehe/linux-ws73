// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "customize-request.h"

void ws73_customize_request_init(struct ws73_customize_request *request)
{
	mutex_init(&request->command_lock);
	spin_lock_init(&request->lock);
	init_completion(&request->done);
	request->state = WS73_CUSTOM_UNUSED;
	request->error = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_request_init, "WS73");

static void ws73_customize_fail_locked(struct ws73_customize_request *request, int error)
{
	if (request->state == WS73_CUSTOM_FAILED)
		return;
	request->error = error < 0 ? error : -ESHUTDOWN;
	request->state = WS73_CUSTOM_FAILED;
	complete_all(&request->done);
}

void ws73_customize_abort(struct ws73_customize_request *request, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&request->lock, flags);
	ws73_customize_fail_locked(request, error);
	spin_unlock_irqrestore(&request->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_abort, "WS73");

int ws73_customize_receive(struct ws73_customize_request *request,
			   enum ws73_notification notification, u64 now_ms)
{
	unsigned long flags;
	int ret = 0;

	if (!request)
		return -EINVAL;
	if (notification != WS73_NOTIFY_CUSTOMIZED)
		return -ENOENT;
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_CUSTOM_FAILED) {
		ret = request->error;
	} else if (request->state == WS73_CUSTOM_UNUSED) {
		ret = -ENOENT;
	} else if (request->state != WS73_CUSTOM_WAITING) {
		ret = -EPROTO;
		ws73_customize_fail_locked(request, ret);
	} else if (now_ms >= request->deadline) {
		ret = -ETIMEDOUT;
		ws73_customize_fail_locked(request, ret);
	} else {
		request->state = WS73_CUSTOM_REPLIED;
		complete_all(&request->done);
	}
	spin_unlock_irqrestore(&request->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_receive, "WS73");

struct ws73_customize_io {
	struct ws73_customize_request *request;
	const struct ws73_usb_tx_io *transport;
};

static u64 ws73_customize_now(void *context)
{
	struct ws73_customize_io *io = context;

	return io->transport->now_ms(io->transport->context);
}

static bool ws73_customize_cancelled(void *context)
{
	struct ws73_customize_io *io = context;
	unsigned long flags;
	bool failed;

	spin_lock_irqsave(&io->request->lock, flags);
	failed = io->request->state == WS73_CUSTOM_FAILED;
	spin_unlock_irqrestore(&io->request->lock, flags);
	return failed || io->transport->cancelled(io->transport->context);
}

static int ws73_customize_send(void *context, const u8 *data, size_t length,
			       unsigned int timeout)
{
	struct ws73_customize_io *io = context;
	struct ws73_customize_request *request = io->request;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_CUSTOM_FAILED) {
		ret = request->error;
	} else if (request->state != WS73_CUSTOM_PREPARING) {
		ret = -EPROTO;
	} else if (length == WS73_USB_TX_PREFIX + WS73_CUSTOMIZE_SIZE &&
		   !get_unaligned_le32(data)) {
		/* Queue-switch/reset traffic cannot authorize an acknowledgment. */
		request->state = WS73_CUSTOM_WAITING;
	}
	spin_unlock_irqrestore(&request->lock, flags);
	if (ret)
		return ret;
	/* Backend must serialize physical admission with device disconnect. */
	return io->transport->send(io->transport->context, data, length, timeout);
}

int ws73_customize_apply(struct ws73_customize_request *request,
			 struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			 const struct ws73_customization *parameters, u64 deadline_ms)
{
	struct ws73_customize_io context = { request, io };
	const struct ws73_usb_tx_io guarded_io = {
		.context = &context, .now_ms = ws73_customize_now,
		.cancelled = ws73_customize_cancelled, .send = ws73_customize_send,
	};
	u8 record[WS73_CUSTOMIZE_SIZE];
	unsigned long flags;
	u64 now;
	int ret;

	if (!request || !tx || !io || !io->now_ms || !io->cancelled || !io->send)
		return -EINVAL;
	ret = ws73_customize_encode(record, sizeof(record), parameters);
	if (ret < 0)
		return ret;
	if (!mutex_trylock(&request->command_lock))
		return -EBUSY;
	spin_lock_irqsave(&request->lock, flags);
	ret = request->state == WS73_CUSTOM_FAILED ? request->error :
		request->state != WS73_CUSTOM_UNUSED ? -EALREADY : 0;
	spin_unlock_irqrestore(&request->lock, flags);
	if (ret)
		goto unlock;
	now = io->now_ms(io->context);
	if (now >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	deadline_ms = now + min_t(u64, deadline_ms - now, 2000);
	spin_lock_irqsave(&request->lock, flags);
	if (request->state == WS73_CUSTOM_FAILED) {
		ret = request->error;
	} else {
		request->deadline = deadline_ms;
		request->state = WS73_CUSTOM_PREPARING;
	}
	spin_unlock_irqrestore(&request->lock, flags);
	if (ret)
		goto unlock;
	ret = ws73_usb_tx_record(tx, &guarded_io, record, sizeof(record), deadline_ms);
	if (ret)
		goto failed;
	now = io->now_ms(io->context);
	if (now >= deadline_ms ||
	    !wait_for_completion_timeout(&request->done, msecs_to_jiffies(deadline_ms - now))) {
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
	if (request->state == WS73_CUSTOM_REPLIED) {
		request->state = WS73_CUSTOM_DONE;
		ret = 0;
	} else {
		ret = request->state == WS73_CUSTOM_FAILED ? request->error : -EPROTO;
	}
	spin_unlock_irqrestore(&request->lock, flags);
	goto unlock;
failed:
	ws73_customize_abort(request, ret);
	spin_lock_irqsave(&request->lock, flags);
	ret = request->error;
	spin_unlock_irqrestore(&request->lock, flags);
unlock:
	mutex_unlock(&request->command_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_customize_apply, "WS73");
