// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/module.h>

#include "startup.h"

void ws73_startup_init(struct ws73_startup *startup)
{
	mutex_init(&startup->command_lock);
	spin_lock_init(&startup->lock);
	init_completion(&startup->bsp_done);
	init_completion(&startup->boot_done);
	ws73_customize_request_init(&startup->customization);
	ws73_ble_control_init(&startup->ble);
	startup->state = WS73_STARTUP_UNUSED;
	startup->error = 0;
	startup->bsp_seen = false;
	startup->boot_seen = false;
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_init, "WS73");

int ws73_startup_arm(struct ws73_startup *startup, u64 now_ms, u64 deadline_ms)
{
	unsigned long flags;
	int ret;

	if (!startup)
		return -EINVAL;
	spin_lock_irqsave(&startup->lock, flags);
	if (startup->state == WS73_STARTUP_FAILED) {
		ret = startup->error;
	} else if (startup->state != WS73_STARTUP_UNUSED) {
		ret = -EALREADY;
	} else if (now_ms >= deadline_ms) {
		ret = -ETIMEDOUT;
		startup->error = ret;
		startup->state = WS73_STARTUP_FAILED;
	} else {
		/* One 12 s budget across all stages, including platform TX; no retries. */
		startup->deadline = now_ms + min_t(u64, deadline_ms - now_ms, 12000);
		startup->state = WS73_STARTUP_ARMED;
		ret = 0;
	}
	spin_unlock_irqrestore(&startup->lock, flags);
	if (ret == -ETIMEDOUT)
		ws73_startup_abort(startup, ret);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_arm, "WS73");

void ws73_startup_abort(struct ws73_startup *startup, int error)
{
	unsigned long flags;

	spin_lock_irqsave(&startup->lock, flags);
	if (startup->state != WS73_STARTUP_FAILED) {
		startup->error = error < 0 ? error : -ESHUTDOWN;
		startup->state = WS73_STARTUP_FAILED;
	}
	error = startup->error;
	complete_all(&startup->bsp_done);
	complete_all(&startup->boot_done);
	spin_unlock_irqrestore(&startup->lock, flags);
	/* Never nest the coordinator state lock with a transaction lock. */
	ws73_customize_abort(&startup->customization, error);
	ws73_ble_control_abort(&startup->ble, error);
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_abort, "WS73");

static int ws73_startup_latch(struct ws73_startup *startup, bool bsp, u64 now)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&startup->lock, flags);
	if (startup->state == WS73_STARTUP_FAILED)
		ret = startup->error;
	else if (startup->state == WS73_STARTUP_UNUSED)
		ret = -ENOENT;
	else if (startup->state == WS73_STARTUP_DONE)
		ret = 0; /* Repeated readiness notifications are idempotent. */
	else if (now >= startup->deadline)
		ret = -ETIMEDOUT;
	else if (bsp) {
		startup->bsp_seen = true;
		complete_all(&startup->bsp_done);
	} else {
		startup->boot_seen = true;
		complete_all(&startup->boot_done);
	}
	spin_unlock_irqrestore(&startup->lock, flags);
	if (ret && ret != -ENOENT)
		ws73_startup_abort(startup, ret);
	return ret;
}

int ws73_startup_interrupt(struct ws73_startup *startup,
			   const struct ws73_interrupt *event, u64 now_ms)
{
	int ret;

	if (!startup || !event)
		return -EINVAL;
	if (event->events & WS73_INTERRUPT_PANIC)
		ret = -EIO;
	else if (event->events & WS73_INTERRUPT_PACKET_LOSS)
		ret = -ENOBUFS;
	else if (event->events & WS73_INTERRUPT_BSP_READY)
		return ws73_startup_latch(startup, true, now_ms);
	else
		return -ENOENT;
	ws73_startup_abort(startup, ret);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_interrupt, "WS73");

int ws73_startup_receive(struct ws73_startup *startup,
			 enum ws73_notification notification, u64 now_ms)
{
	int ret;

	if (!startup)
		return -EINVAL;
	if (notification == WS73_NOTIFY_BOOT)
		return ws73_startup_latch(startup, false, now_ms);
	if (notification == WS73_NOTIFY_CUSTOMIZED)
		ret = ws73_customize_receive(&startup->customization, notification, now_ms);
	else
		ret = ws73_ble_control_receive(&startup->ble, notification, now_ms);
	if (ret && ret != -ENOENT)
		ws73_startup_abort(startup, ret);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_receive, "WS73");

static int ws73_startup_stage(struct ws73_startup *startup, enum ws73_startup_state stage)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&startup->lock, flags);
	ret = startup->state == WS73_STARTUP_FAILED ? startup->error : 0;
	if (!ret)
		startup->state = stage;
	spin_unlock_irqrestore(&startup->lock, flags);
	return ret;
}

static int ws73_startup_wait(struct ws73_startup *startup, struct completion *done,
			     const struct ws73_usb_tx_io *io, unsigned int limit)
{
	u64 now = io->now_ms(io->context);
	u64 deadline;

	if (io->cancelled(io->context))
		return -ENODEV;
	if (now >= startup->deadline)
		return -ETIMEDOUT;
	deadline = now + min_t(u64, startup->deadline - now, limit);
	if (!wait_for_completion_timeout(done, msecs_to_jiffies(deadline - now)))
		return -ETIMEDOUT;
	if (io->cancelled(io->context))
		return -ENODEV;
	if (io->now_ms(io->context) >= deadline)
		return -ETIMEDOUT;
	/* A failure wakeup is checked atomically by the following stage change. */
	return 0;
}

int ws73_startup_run(struct ws73_startup *startup, struct ws73_usb_tx *tx,
		     const struct ws73_usb_tx_io *bulk,
		     const struct ws73_ble_control_io *control,
		     const struct ws73_platform_config *platform,
		     const struct ws73_customization *parameters)
{
	unsigned long flags;
	int ret;

	if (!startup || !tx || !bulk || !bulk->now_ms || !bulk->cancelled || !bulk->send ||
	    !control || !control->now_ms || !control->cancelled || !control->send || !platform || !parameters)
		return -EINVAL;
	if (parameters->calibration_count > 8 || platform->wake_gpio > U8_MAX)
		return -ERANGE;
	if (!mutex_trylock(&startup->command_lock))
		return -EBUSY;
	spin_lock_irqsave(&startup->lock, flags);
	if (startup->state == WS73_STARTUP_FAILED) {
		ret = startup->error;
	} else if (startup->state == WS73_STARTUP_UNUSED) {
		ret = -EAGAIN;
	} else if (startup->state != WS73_STARTUP_ARMED) {
		ret = -EALREADY;
	} else {
		startup->state = WS73_STARTUP_WAIT_BSP;
		ret = 0;
	}
	spin_unlock_irqrestore(&startup->lock, flags);
	if (ret)
		goto unlock;
	ret = ws73_startup_wait(startup, &startup->bsp_done, bulk, 6000);
	if (ret)
		goto failed;
	ret = ws73_startup_stage(startup, WS73_STARTUP_CONFIGURE);
	if (ret)
		goto failed;
	ret = ws73_platform_configure(tx, bulk, platform, startup->deadline);
	if (ret)
		goto failed;
	ret = ws73_startup_stage(startup, WS73_STARTUP_CUSTOMIZE);
	if (ret)
		goto failed;
	ret = ws73_customize_apply(&startup->customization, tx, bulk,
				   parameters, startup->deadline);
	if (ret)
		goto failed;
	ret = ws73_startup_stage(startup, WS73_STARTUP_ENABLE);
	if (ret)
		goto failed;
	ret = ws73_ble_control_set(&startup->ble, control, true, startup->deadline);
	if (ret)
		goto failed;
	ret = ws73_startup_stage(startup, WS73_STARTUP_WAIT_BOOT);
	if (ret)
		goto failed;
	ret = ws73_startup_wait(startup, &startup->boot_done, bulk, 2000);
	if (ret)
		goto failed;
	ret = ws73_startup_stage(startup, WS73_STARTUP_DONE);
	if (!ret)
		goto unlock;
failed:
	ws73_startup_abort(startup, ret);
	spin_lock_irqsave(&startup->lock, flags);
	ret = startup->error;
	spin_unlock_irqrestore(&startup->lock, flags);
unlock:
	mutex_unlock(&startup->command_lock);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_startup_run, "WS73");
