// SPDX-License-Identifier: GPL-2.0-only
/* WS73 boot/runtime identity observations. Firmware loading remains gated. */
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/unaligned.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

#include "boot.h"
#include "startup.h"
#include "runtime.h"

struct ws73_usb {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct usb_anchor submitted;
	struct work_struct initialize;
	struct work_struct stop;
	struct ws73_usb_tx tx;
	struct ws73_platform_request identity;
	struct ws73_startup startup;
	struct ws73_ble_control_io ble_io;
	struct ws73_boot_io boot_io;
	struct urb *rx_urb;
	struct urb *interrupt_urb;
	u8 *rx_buffer;
	u8 *interrupt_buffer;
	spinlock_t lock; /* Serialize submission admission with disconnect. */
	bool disconnected;
	bool runtime;
	int error;
	u8 in;
	u8 out;
	u8 notifications_seen; /* Observations only; never used to enable a function. */
	bool bsp_seen;
};

static void ws73_usb_stop(struct work_struct *work)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, stop);

	usb_kill_anchored_urbs(&ws->submitted);
	/* The RX completion can already have left the anchor. Join it too. */
	usb_kill_urb(ws->rx_urb);
	usb_kill_urb(ws->interrupt_urb);
}

static void ws73_usb_fail(struct ws73_usb *ws, int error)
{
	unsigned long flags;
	bool first = false;

	spin_lock_irqsave(&ws->lock, flags);
	if (!ws->error && !ws->disconnected) {
		WRITE_ONCE(ws->error, error);
		first = true;
	}
	spin_unlock_irqrestore(&ws->lock, flags);
	ws73_platform_abort(&ws->identity, error);
	ws73_startup_abort(&ws->startup, error);
	if (first)
		schedule_work(&ws->stop);
}

struct ws73_usb_request {
	struct completion done;
	int status;
	unsigned int actual;
};

static void ws73_usb_complete(struct urb *urb)
{
	struct ws73_usb_request *request = urb->context;

	request->status = urb->status;
	request->actual = urb->actual_length;
	complete(&request->done);
}

static int ws73_usb_transfer(struct ws73_usb *ws, void *data, size_t size,
			     unsigned int timeout_ms, bool receive, bool control)
{
	struct ws73_usb_request request;
	struct usb_ctrlrequest *setup = NULL;
	size_t limit = ws->runtime ? WS73_USB_TX_MAX :
		       receive ? 512 : WS73_BOOT_CHUNK_SIZE;
	struct urb *urb;
	unsigned long flags;
	u8 *buffer;
	int ret;

	if (!data || !size || size > limit ||
	    !timeout_ms || timeout_ms > 2000)
		return -EINVAL;
	if (control && (!ws->runtime || receive || size != 4))
		return -EINVAL;
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		return -ENOMEM;
	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto free_urb;
	}
	if (!receive)
		memcpy(buffer, data, size);
	init_completion(&request.done);
	request.status = -EINPROGRESS;
	request.actual = 0;
	if (control) {
		setup = kzalloc_obj(*setup);
		if (!setup) {
			ret = -ENOMEM;
			goto free_buffer;
		}
		setup->bRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		setup->wLength = cpu_to_le16(4);
		/* SDK110: request/value/index are all zero; only interface 0 binds. */
		usb_fill_control_urb(urb, ws->udev, usb_sndctrlpipe(ws->udev, 0),
				     (u8 *)setup, buffer, size, ws73_usb_complete, &request);
	} else {
		usb_fill_bulk_urb(urb, ws->udev,
				  receive ? usb_rcvbulkpipe(ws->udev, ws->in) :
				 usb_sndbulkpipe(ws->udev, ws->out),
				 buffer, size, ws73_usb_complete, &request);
		if (ws->runtime && !receive)
			urb->transfer_flags |= URB_ZERO_PACKET;
	}

	spin_lock_irqsave(&ws->lock, flags);
	if (ws->disconnected || ws->error) {
		ret = ws->disconnected ? -ENODEV : ws->error;
	} else {
		usb_anchor_urb(urb, &ws->submitted);
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret)
			usb_unanchor_urb(urb);
	}
	spin_unlock_irqrestore(&ws->lock, flags);
	if (ret)
		goto free_buffer;

	if (!wait_for_completion_timeout(&request.done, msecs_to_jiffies(timeout_ms)))
		ret = -ETIMEDOUT;
	/* Join completion even if disconnect already removed it from the anchor. */
	usb_kill_urb(urb);
	if (!ret)
		ret = request.status;
	if (!ret && request.actual > size)
		ret = -EOVERFLOW;
	if (!ret) {
		if (receive)
			memcpy(data, buffer, request.actual);
		ret = request.actual;
	}
free_buffer:
	kfree(setup);
	kfree(buffer);
free_urb:
	usb_free_urb(urb);
	return ret;
}

static u64 ws73_usb_now(void *context)
{
	return ktime_to_ms(ktime_get_boottime());
}

static bool ws73_usb_cancelled(void *context)
{
	struct ws73_usb *ws = context;

	return READ_ONCE(ws->disconnected) || READ_ONCE(ws->error);
}

static int ws73_usb_submit_rx(struct ws73_usb *ws, struct urb *urb)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ws->lock, flags);
	if (ws->disconnected || ws->error) {
		ret = ws->disconnected ? -ENODEV : ws->error;
	} else {
		usb_anchor_urb(urb, &ws->submitted);
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret)
			usb_unanchor_urb(urb);
	}
	spin_unlock_irqrestore(&ws->lock, flags);
	return ret;
}

static void ws73_usb_interrupt_complete(struct urb *urb)
{
	struct ws73_usb *ws = urb->context;
	struct ws73_interrupt notification;
	int ret;

	if (ws73_usb_cancelled(ws))
		return;
	ret = urb->status;
	if (!ret)
		ret = ws73_interrupt_decode(ws->interrupt_buffer, urb->actual_length,
					    &notification);
	if (ret)
		goto fail;
	/* A fault in the same word always takes precedence over BSP_READY. */
	if (notification.events & WS73_INTERRUPT_PANIC) {
		ret = -EIO;
		goto fail;
	}
	if (notification.events & WS73_INTERRUPT_PACKET_LOSS) {
		/* Lost high-priority messages cannot prove a successful handshake. */
		ret = -ENOBUFS;
		goto fail;
	}
	ret = ws73_startup_interrupt(&ws->startup, &notification, ws73_usb_now(ws));
	if (ret && ret != -ENOENT)
		dev_dbg_ratelimited(&ws->interface->dev, "startup BSP rejected: %d\n", ret);
	if ((notification.events & WS73_INTERRUPT_BSP_READY) && !ws->bsp_seen) {
		ws->bsp_seen = true;
		dev_info(&ws->interface->dev,
			 "BSP notification observation; pool %#010x is not a TX credit\n",
			 notification.high_priority_pool);
	}
	/* Other bits do not imply BLE action completion, PM success or credits. */
	ret = ws73_usb_submit_rx(ws, urb);
	if (!ret || ws73_usb_cancelled(ws))
		return;
fail:
	ws73_usb_fail(ws, ret);
	dev_err(&ws->interface->dev, "interrupt observation failed: %d\n", ret);
}

static void ws73_usb_rx_complete(struct urb *urb)
{
	struct ws73_usb *ws = urb->context;
	struct ws73_usb_rx rx;
	unsigned int i;
	int ret;

	if (ws73_usb_cancelled(ws))
		return;
	if (urb->status) {
		ws73_usb_fail(ws, urb->status);
		return;
	}
	ret = ws73_usb_aggregate_decode(ws->rx_buffer, urb->actual_length, &rx);
	if (ret && ret != -EOPNOTSUPP) {
		ws73_usb_fail(ws, ret);
		return;
	}
	if (!ret && rx.queue == 9) {
		for (i = 0; i < rx.count; i++) {
			ret = ws73_platform_receive(&ws->identity, rx.record[i].data,
						    rx.record[i].len, ws73_usb_now(ws));
			if (ret && ret != -ENOENT && ret != -EALREADY) {
				ws73_usb_fail(ws, ret);
				return;
			}
		}
	}
	if (!ret && rx.queue == 10) {
		/* The decoder validated every record before publishing any status. */
		for (i = 0; i < rx.count; i++) {
			u8 bit = BIT(rx.notification[i]);

			ret = ws73_startup_receive(&ws->startup, rx.notification[i],
						       ws73_usb_now(ws));
			if (ret && ret != -ENOENT) {
				/* An action failure belongs to BLE, not an active sibling. */
				dev_dbg_ratelimited(&ws->interface->dev,
						    "startup reply rejected: %d\n", ret);
				continue;
			}
			if (ws->notifications_seen & bit)
				continue;
			ws->notifications_seen |= bit;
			dev_info(&ws->interface->dev,
				 "function notification observation %u; no request completion\n",
				 rx.notification[i]);
		}
	}
	/* No function consumers yet. No borrowed URB storage escapes this call. */
	ret = ws73_usb_submit_rx(ws, urb);
	if (ret && !ws73_usb_cancelled(ws))
		ws73_usb_fail(ws, ret);
}

static int ws73_usb_send(void *context, const u8 *data, size_t size, unsigned int timeout)
{
	return ws73_usb_transfer(context, (void *)data, size, timeout, false, false);
}

static int ws73_usb_receive(void *context, u8 *data, size_t size, unsigned int timeout)
{
	return ws73_usb_transfer(context, data, size, timeout, true, false);
}

static int ws73_usb_ble_send(void *context, const u8 command[4], unsigned int timeout)
{
	u32 message = get_unaligned_le32(command);

	if (message != 25 && message != 26)
		return -EINVAL;
	return ws73_usb_transfer(context, (void *)command, 4, timeout, false, true);
}

static void ws73_usb_wait_ready(void *context)
{
	/* SDK requires at least 5 ms between READY and the payload submission. */
	usleep_range(5000, 6000);
	/* The boot executor rechecks cancellation/deadline before sending. */
}

static void ws73_usb_initialize(struct work_struct *work)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, initialize);
	struct ws73_chip_observation observation;
	struct btws73_chip_identity identity;
	struct ws73_version version;
	const struct ws73_usb_tx_io runtime_io = {
		.context = ws,
		.now_ms = ws73_usb_now,
		.cancelled = ws73_usb_cancelled,
		.send = ws73_usb_send,
	};
	int ret;

	if (ws->runtime) {
		u64 deadline = ws73_usb_now(ws) + 4000;
		const char *stage = "chip";

		ret = ws73_usb_submit_rx(ws, ws->interrupt_urb);
		if (!ret)
			ret = ws73_usb_submit_rx(ws, ws->rx_urb);
		if (!ret)
			ret = ws73_platform_get_chip(&ws->identity, &ws->tx, &runtime_io,
						     deadline, &identity);
		if (!ret) {
			stage = "version";
			ret = ws73_platform_get_version(&ws->identity, &ws->tx, &runtime_io,
							deadline, &version);
		}
		if (ret) {
			ws73_usb_fail(ws, ret);
			dev_err(&ws->interface->dev, "runtime %s observation failed: %d\n",
				stage, ret);
			return;
		}
		if (!ws73_usb_cancelled(ws)) {
			dev_info(&ws->interface->dev,
				 "runtime chip observation: package %u bus %u; firmware compatibility unverified\n",
				 identity.package, identity.bus);
			dev_info(&ws->interface->dev, "runtime version observation: firmware %#010x ROM %#010x\n",
				 version.firmware, version.rom);
		}
		return;
	}
	ret = ws73_boot_read_chip(&ws->boot_io, ws73_usb_now(ws) + 4000, &observation);
	if (ret) {
		dev_err(&ws->interface->dev, "boot chip observation failed: %d\n", ret);
		return;
	}
	if (ws73_usb_cancelled(ws))
		return;
	dev_info(&ws->interface->dev,
		 "boot chip observation %#010x: package %u bus %u; cold-read qualification pending\n",
		 observation.raw, observation.chip.package, observation.chip.bus);
	/* No profile selection, calibration writes, download or child registration. */
}

static int ws73_usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_host_interface *alt = interface->cur_altsetting;
	struct usb_device *udev = interface_to_usbdev(interface);
	struct ws73_usb *ws;
	unsigned int i;
	bool runtime = alt->desc.bNumEndpoints == 5;

	/* Match the SDK boot endpoint roles as well as the interface protocol. */
	if (interface->num_altsetting != 1 || alt->desc.bInterfaceNumber ||
	    alt->desc.bAlternateSetting || (!runtime && alt->desc.bNumEndpoints != 2) ||
	    alt->desc.bInterfaceClass != 0xe0 || alt->desc.bInterfaceSubClass != 0x02 ||
	    alt->desc.bInterfaceProtocol != 0x02 ||
	    !usb_endpoint_is_bulk_in(&alt->endpoint[0].desc) ||
	    !usb_endpoint_is_bulk_out(&alt->endpoint[1].desc))
		return -ENODEV;
	if (runtime && (!usb_endpoint_is_int_in(&alt->endpoint[2].desc) ||
			!usb_endpoint_is_bulk_out(&alt->endpoint[3].desc) ||
			!usb_endpoint_is_bulk_in(&alt->endpoint[4].desc) ||
			usb_endpoint_maxp(&alt->endpoint[2].desc) < 8 ||
			!alt->endpoint[2].desc.bInterval))
		return -ENODEV;
	for (i = 0; i < alt->desc.bNumEndpoints; i++)
		if (!usb_endpoint_maxp(&alt->endpoint[i].desc))
			return -ENODEV;
	if ((udev->speed != USB_SPEED_FULL && udev->speed != USB_SPEED_HIGH) ||
	    !usb_endpoint_maxp(&alt->endpoint[0].desc) ||
	    !usb_endpoint_maxp(&alt->endpoint[1].desc))
		return -ENODEV;

	ws = kzalloc_obj(*ws);
	if (!ws)
		return -ENOMEM;
	ws->udev = usb_get_dev(udev);
	ws->interface = interface;
	ws->runtime = runtime;
	ws->in = alt->endpoint[0].desc.bEndpointAddress;
	ws->out = alt->endpoint[1].desc.bEndpointAddress;
	spin_lock_init(&ws->lock);
	init_usb_anchor(&ws->submitted);
	INIT_WORK(&ws->initialize, ws73_usb_initialize);
	INIT_WORK(&ws->stop, ws73_usb_stop);
	ws73_usb_tx_init(&ws->tx);
	ws73_platform_init(&ws->identity);
	ws73_startup_init(&ws->startup);
	ws->boot_io = (struct ws73_boot_io) {
		.context = ws,
		.now_ms = ws73_usb_now,
		.cancelled = ws73_usb_cancelled,
		.send = ws73_usb_send,
		.receive = ws73_usb_receive,
		.wait_ready = ws73_usb_wait_ready,
	};
	ws->ble_io = (struct ws73_ble_control_io) {
		.context = ws,
		.now_ms = ws73_usb_now,
		.cancelled = ws73_usb_cancelled,
		.send = ws73_usb_ble_send,
	};
	if (runtime) {
		ws->rx_urb = usb_alloc_urb(0, GFP_KERNEL);
		ws->interrupt_urb = usb_alloc_urb(0, GFP_KERNEL);
		ws->rx_buffer = kmalloc(BTWS73_RX_MAX_SIZE, GFP_KERNEL);
		ws->interrupt_buffer = kmalloc(WS73_INTERRUPT_SIZE, GFP_KERNEL);
		if (!ws->rx_urb || !ws->rx_buffer || !ws->interrupt_urb ||
		    !ws->interrupt_buffer) {
			usb_free_urb(ws->rx_urb);
			usb_free_urb(ws->interrupt_urb);
			kfree(ws->rx_buffer);
			kfree(ws->interrupt_buffer);
			usb_put_dev(ws->udev);
			kfree(ws);
			return -ENOMEM;
		}
		usb_fill_bulk_urb(ws->rx_urb, ws->udev, usb_rcvbulkpipe(ws->udev, ws->in),
				  ws->rx_buffer, BTWS73_RX_MAX_SIZE, ws73_usb_rx_complete, ws);
		usb_fill_int_urb(ws->interrupt_urb, ws->udev,
				 usb_rcvintpipe(ws->udev, alt->endpoint[2].desc.bEndpointAddress),
				 ws->interrupt_buffer, WS73_INTERRUPT_SIZE,
				 ws73_usb_interrupt_complete, ws, alt->endpoint[2].desc.bInterval);
	}
	usb_set_intfdata(interface, ws);
	schedule_work(&ws->initialize);
	return 0;
}

static void ws73_usb_disconnect(struct usb_interface *interface)
{
	struct ws73_usb *ws = usb_get_intfdata(interface);
	unsigned long flags;

	usb_set_intfdata(interface, NULL);
	spin_lock_irqsave(&ws->lock, flags);
	WRITE_ONCE(ws->disconnected, true);
	spin_unlock_irqrestore(&ws->lock, flags);
	ws73_platform_abort(&ws->identity, -ENODEV);
	ws73_startup_abort(&ws->startup, -ENODEV);
	usb_kill_anchored_urbs(&ws->submitted);
	usb_kill_urb(ws->rx_urb);
	usb_kill_urb(ws->interrupt_urb);
	cancel_work_sync(&ws->initialize);
	cancel_work_sync(&ws->stop);
	dev_dbg(&interface->dev, "device callbacks drained\n");
	usb_free_urb(ws->rx_urb);
	usb_free_urb(ws->interrupt_urb);
	kfree(ws->rx_buffer);
	kfree(ws->interrupt_buffer);
	usb_put_dev(ws->udev);
	kfree(ws);
}

static const struct usb_device_id ws73_usb_ids[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0xffff, 0x3733, 0xe0, 0x02, 0x02) },
	{}
};
MODULE_DEVICE_TABLE(usb, ws73_usb_ids);

static struct usb_driver ws73_usb_driver = {
	.name = "ws73",
	.probe = ws73_usb_probe,
	.disconnect = ws73_usb_disconnect,
	.id_table = ws73_usb_ids,
};
module_usb_driver(ws73_usb_driver);

MODULE_DESCRIPTION("WS73 USB boot and runtime identity observations");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
