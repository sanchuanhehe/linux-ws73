// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/unaligned.h>

#include "ble-control.h"

void ws73_ble_control_init(struct ws73_ble_control *control)
{
	mutex_init(&control->command_lock);
	spin_lock_init(&control->lock);
	init_completion(&control->done);
	control->state = WS73_BLE_UNKNOWN;
	control->error = 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_control_init, "WS73");

static void ws73_ble_control_fail_locked(struct ws73_ble_control *control, int error)
{
	if (control->state == WS73_BLE_FAILED)
		return;
	control->error = error < 0 ? error : -ESHUTDOWN;
	control->state = WS73_BLE_FAILED;
	complete_all(&control->done);
}

void ws73_ble_control_abort(struct ws73_ble_control *control, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&control->lock, flags);
	ws73_ble_control_fail_locked(control, error);
	spin_unlock_irqrestore(&control->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_control_abort, "WS73");

int ws73_ble_control_receive(struct ws73_ble_control *control,
			     enum ws73_notification notification, u64 now_ms)
{
	unsigned long flags;
	int ret = 0;

	if (!control)
		return -EINVAL;
	if (notification != WS73_NOTIFY_BLE_OPEN && notification != WS73_NOTIFY_BLE_CLOSE)
		return -ENOENT;
	spin_lock_irqsave(&control->lock, flags);
	if (control->state == WS73_BLE_FAILED) {
		ret = control->error;
	} else if (control->state == WS73_BLE_QUIESCED) {
		ret = -ESHUTDOWN;
	} else if (control->state == WS73_BLE_UNKNOWN) {
		ret = -ENOENT; /* An unarmed observation is not an action completion. */
	} else if (control->state != WS73_BLE_WAITING ||
		   control->target != (notification == WS73_NOTIFY_BLE_OPEN)) {
		ret = -EPROTO;
		ws73_ble_control_fail_locked(control, ret);
	} else if (now_ms >= control->deadline) {
		ret = -ETIMEDOUT;
		ws73_ble_control_fail_locked(control, ret);
	} else {
		control->state = WS73_BLE_REPLIED;
		complete_all(&control->done);
	}
	spin_unlock_irqrestore(&control->lock, flags);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_control_receive, "WS73");

static int ws73_ble_control_error(struct ws73_ble_control *control,
				  const struct ws73_ble_control_io *io)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&control->lock, flags);
	ret = control->state == WS73_BLE_FAILED ? control->error : 0;
	spin_unlock_irqrestore(&control->lock, flags);
	if (!ret && io->cancelled(io->context))
		ret = -ENODEV;
	return ret;
}

static int ws73_ble_control_request(struct ws73_ble_control *control,
				    const struct ws73_ble_control_io *io,
				    bool enabled, bool quiesce, u64 deadline_ms)
{
	unsigned long flags;
	u8 command[4];
	u64 now;
	int ret;

	if (!control || !io || !io->now_ms || !io->cancelled || !io->send)
		return -EINVAL;
	if (!mutex_trylock(&control->command_lock))
		return -EBUSY;
	spin_lock_irqsave(&control->lock, flags);
	if (control->state == WS73_BLE_FAILED)
		ret = control->error;
	else if (control->state == WS73_BLE_QUIESCED)
		ret = -ESHUTDOWN;
	else if (quiesce && control->state != WS73_BLE_UNKNOWN)
		ret = -EBUSY;
	else if (control->state == WS73_BLE_UNKNOWN && !enabled && !quiesce)
		ret = -EAGAIN;
	else if ((control->state == WS73_BLE_ENABLED && enabled) ||
		 (control->state == WS73_BLE_DISABLED && !enabled))
		ret = -EALREADY;
	else
		ret = 0;
	spin_unlock_irqrestore(&control->lock, flags);
	if (ret)
		goto unlock;
	now = io->now_ms(io->context);
	if (now >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	/* Subtract before addition: a distant deadline cannot wrap this cap. */
	deadline_ms = now + min_t(u64, deadline_ms - now, 2000);
	spin_lock_irqsave(&control->lock, flags);
	if (control->state == WS73_BLE_FAILED) {
		ret = control->error;
	} else {
		control->target = enabled;
		control->deadline = deadline_ms;
		reinit_completion(&control->done);
		control->state = WS73_BLE_WAITING;
	}
	spin_unlock_irqrestore(&control->lock, flags);
	if (ret)
		goto unlock;
	now = io->now_ms(io->context);
	if (now >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	ret = ws73_ble_control_error(control, io);
	if (ret)
		goto failed;
	put_unaligned_le32(enabled ? 25 : 26, command);
	ret = io->send(io->context, command, deadline_ms - now);
	if (ret != sizeof(command)) {
		if (ret >= 0)
			ret = -EMSGSIZE;
		goto failed;
	}
	now = io->now_ms(io->context);
	if (now >= deadline_ms ||
	    !wait_for_completion_timeout(&control->done, msecs_to_jiffies(deadline_ms - now))) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	ret = ws73_ble_control_error(control, io);
	if (ret)
		goto failed;
	if (io->now_ms(io->context) >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto failed;
	}
	spin_lock_irqsave(&control->lock, flags);
	if (control->state == WS73_BLE_REPLIED) {
		control->state = quiesce ? WS73_BLE_QUIESCED :
				 enabled ? WS73_BLE_ENABLED : WS73_BLE_DISABLED;
		ret = 0;
	} else {
		ret = control->state == WS73_BLE_FAILED ? control->error : -EPROTO;
	}
	spin_unlock_irqrestore(&control->lock, flags);
	goto unlock;
failed:
	ws73_ble_control_abort(control, ret);
	spin_lock_irqsave(&control->lock, flags);
	ret = control->error;
	spin_unlock_irqrestore(&control->lock, flags);
unlock:
	mutex_unlock(&control->command_lock);
	return ret;
}

int ws73_ble_control_set(struct ws73_ble_control *control,
			 const struct ws73_ble_control_io *io, bool enabled,
			 u64 deadline_ms)
{
	return ws73_ble_control_request(control, io, enabled, false, deadline_ms);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_control_set, "WS73");

int ws73_ble_control_quiesce(struct ws73_ble_control *control,
			     const struct ws73_ble_control_io *io, u64 deadline_ms)
{
	return ws73_ble_control_request(control, io, false, true, deadline_ms);
}
EXPORT_SYMBOL_NS_GPL(ws73_ble_control_quiesce, "WS73");
