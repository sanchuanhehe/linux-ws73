// SPDX-License-Identifier: GPL-2.0-only
#include <linux/err.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "ble-link.h"

static u64 ws73_ble_link_deadline(u64 now)
{
	return now + min_t(u64, U64_MAX - now, 2000);
}

static int ws73_ble_link_set(void *context, bool enabled)
{
	struct ws73_ble_link *link = context;
	u64 now = link->control.now_ms(link->control.context);
	int ret;

	ret = ws73_ble_control_set(&link->startup->ble, &link->control, enabled,
				   ws73_ble_link_deadline(now));
	if (ret)
		ws73_ble_link_abort(link, ret);
	return ret;
}

static int ws73_ble_link_send(void *context, const u8 *record, size_t length)
{
	struct ws73_ble_link *link = context;
	u64 now = link->bulk.now_ms(link->bulk.context);
	int ret;

	ret = ws73_usb_tx_record(link->tx, &link->bulk, record, length,
				 ws73_ble_link_deadline(now));
	/* Shared queue sequencing is terminal too; all physical consumers suffer. */
	if (ret)
		link->transport_fault(link->fault_context, ret);
	return ret;
}

static const struct ws73_ble_backend ws73_ble_link_backend = {
	.set_enabled = ws73_ble_link_set,
	.send = ws73_ble_link_send,
};

void ws73_ble_link_init(struct ws73_ble_link *link, struct ws73_startup *startup,
			struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *bulk,
			const struct ws73_ble_control_io *control,
			void (*transport_fault)(void *, int), void *context)
{
	spin_lock_init(&link->lock);
	link->child = NULL;
	link->startup = startup;
	link->tx = tx;
	link->bulk = *bulk;
	link->control = *control;
	link->transport_fault = transport_fault;
	link->fault_context = context;
	link->error = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_init, "WS73");

void ws73_ble_link_abort(struct ws73_ble_link *link, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&link->lock, flags);
	if (!link->error)
		link->error = error < 0 ? error : -ESHUTDOWN;
	if (link->child)
		ws73_ble_fault(link->child, link->error);
	error = link->error;
	spin_unlock_irqrestore(&link->lock, flags);
	ws73_ble_control_abort(&link->startup->ble, error);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_abort, "WS73");

int ws73_ble_link_publish(struct ws73_ble_link *link, struct device *parent)
{
	struct auxiliary_device *child;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&link->lock, flags);
	ret = link->error ?: (link->child ? -EALREADY : 0);
	spin_unlock_irqrestore(&link->lock, flags);
	if (ret)
		return ret;
	spin_lock_irqsave(&link->startup->lock, flags);
	ret = link->startup->state == WS73_STARTUP_DONE ? 0 : -EAGAIN;
	spin_unlock_irqrestore(&link->startup->lock, flags);
	if (ret)
		return ret;
	spin_lock_irqsave(&link->startup->ble.lock, flags);
	ret = link->startup->ble.state == WS73_BLE_DISABLED ? 0 : -EAGAIN;
	spin_unlock_irqrestore(&link->startup->ble.lock, flags);
	if (ret)
		return ret;
	child = ws73_ble_prepare(parent, &ws73_ble_link_backend, link);
	if (IS_ERR(child))
		return PTR_ERR(child);
	spin_lock_irqsave(&link->lock, flags);
	ret = link->error ?: (link->child ? -EALREADY : 0);
	if (!ret)
		link->child = child;
	spin_unlock_irqrestore(&link->lock, flags);
	if (ret)
		goto discard;
	/* Routing is installed before add: an immediate consumer can receive. */
	ret = auxiliary_device_add(child);
	if (!ret)
		return 0;
	spin_lock_irqsave(&link->lock, flags);
	link->child = NULL;
	spin_unlock_irqrestore(&link->lock, flags);
discard:
	ws73_ble_discard(child);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_publish, "WS73");

void ws73_ble_link_remove(struct ws73_ble_link *link)
{
	struct auxiliary_device *child;
	unsigned long flags;

	ws73_ble_link_abort(link, -ENODEV);
	spin_lock_irqsave(&link->lock, flags);
	child = link->child;
	link->child = NULL;
	spin_unlock_irqrestore(&link->lock, flags);
	if (child)
		ws73_ble_remove(child);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_remove, "WS73");

void ws73_ble_link_receive(struct ws73_ble_link *link, const struct ws73_usb_rx *rx)
{
	unsigned long flags;
	unsigned int i;

	if (rx->queue != 7)
		return;
	spin_lock_irqsave(&link->lock, flags);
	if (!link->error && link->child)
		for (i = 0; i < rx->count; i++)
			ws73_ble_receive(link->child, rx->record[i].data + 4,
					 get_unaligned_le16(rx->record[i].data + 2));
	spin_unlock_irqrestore(&link->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_receive, "WS73");

int ws73_ble_link_notify(struct ws73_ble_link *link, enum ws73_notification event, u64 now)
{
	unsigned long flags;
	bool started;
	int ret;

	spin_lock_irqsave(&link->startup->lock, flags);
	started = link->startup->state == WS73_STARTUP_DONE;
	spin_unlock_irqrestore(&link->startup->lock, flags);
	if (!started || event == WS73_NOTIFY_BOOT || event == WS73_NOTIFY_CUSTOMIZED)
		return ws73_startup_receive(link->startup, event, now);
	ret = ws73_ble_control_receive(&link->startup->ble, event, now);
	if (ret && ret != -ENOENT)
		ws73_ble_link_abort(link, ret);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_link_notify, "WS73");
