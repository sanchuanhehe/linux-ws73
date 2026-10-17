// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/module.h>

#include "usb-tx.h"

void ws73_usb_tx_init(struct ws73_usb_tx *tx)
{
	mutex_init(&tx->lock);
	tx->sequence = 0;
	tx->queue = 0;
	tx->queue_valid = false;
	tx->failure = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_tx_init, "WS73");

static int ws73_usb_tx_remaining(const struct ws73_usb_tx_io *io, u64 deadline)
{
	u64 now;

	if (io->cancelled(io->context))
		return -ENODEV;
	now = io->now_ms(io->context);
	if (now >= deadline)
		return -ETIMEDOUT;
	return min_t(u64, deadline - now, 2000);
}

static int ws73_usb_tx_send(struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			    size_t length, u64 deadline)
{
	int ret, timeout;

	timeout = ws73_usb_tx_remaining(io, deadline);
	if (timeout < 0)
		return timeout;
	/* The sequence is a wrapping USB counter, not a response-match token. */
	tx->sequence++;
	ret = io->send(io->context, tx->buffer, length, timeout);
	if (ret < 0)
		return ret;
	if (ret != length)
		return -EREMOTEIO;
	ret = ws73_usb_tx_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

int ws73_usb_tx_record(struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			const u8 *record, size_t length, u64 deadline_ms)
{
	int ret, size;
	u8 queue;

	if (!tx || !io || !io->now_ms || !io->cancelled || !io->send || !record)
		return -EINVAL;
	mutex_lock(&tx->lock);
	if (tx->failure) {
		ret = tx->failure;
		goto unlock;
	}
	/* Validate the whole input before any queue-control side effect. */
	ret = ws73_usb_single_encode(tx->buffer, sizeof(tx->buffer),
				     tx->sequence, record, length);
	if (ret < 0)
		goto unlock;
	queue = record[1];
	if (!tx->queue_valid || tx->queue != queue) {
		size = ws73_usb_control_encode(tx->buffer, sizeof(tx->buffer),
					       tx->sequence, WS73_USB_SWITCH_QUEUE, queue);
		if (size < 0) {
			ret = size;
			goto failed;
		}
		ret = ws73_usb_tx_send(tx, io, size, deadline_ms);
		if (ret)
			goto failed;
		size = ws73_usb_control_encode(tx->buffer, sizeof(tx->buffer),
					       tx->sequence, WS73_USB_RESET_AGGREGATION, queue);
		if (size < 0) {
			ret = size;
			goto failed;
		}
		ret = ws73_usb_tx_send(tx, io, size, deadline_ms);
		if (ret)
			goto failed;
		tx->queue = queue;
		tx->queue_valid = true;
	}
	size = ws73_usb_single_encode(tx->buffer, sizeof(tx->buffer),
				      tx->sequence, record, length);
	if (size < 0) {
		ret = size;
		goto failed;
	}
	ret = ws73_usb_tx_send(tx, io, size, deadline_ms);
	if (!ret)
		goto unlock;
failed:
	tx->failure = ret;
	tx->queue_valid = false;
unlock:
	mutex_unlock(&tx->lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_tx_record, "WS73");
