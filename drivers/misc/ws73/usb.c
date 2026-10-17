// SPDX-License-Identifier: GPL-2.0-only
/* WS73 boot identity observation. Firmware loading remains gated. */
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

#include "boot.h"

struct ws73_usb {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct usb_anchor submitted;
	struct work_struct initialize;
	spinlock_t lock; /* Serialize submission admission with disconnect. */
	bool disconnected;
	u8 in;
	u8 out;
};

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
			     unsigned int timeout_ms, bool receive)
{
	struct ws73_usb_request request;
	struct urb *urb;
	unsigned long flags;
	u8 *buffer;
	int ret;

	if (!size || size > 512 || !timeout_ms || timeout_ms > 2000)
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
	usb_fill_bulk_urb(urb, ws->udev,
			  receive ? usb_rcvbulkpipe(ws->udev, ws->in) :
			 usb_sndbulkpipe(ws->udev, ws->out),
			 buffer, size, ws73_usb_complete, &request);

	spin_lock_irqsave(&ws->lock, flags);
	if (ws->disconnected) {
		ret = -ENODEV;
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

	return READ_ONCE(ws->disconnected);
}

static int ws73_usb_send(void *context, const u8 *data, size_t size, unsigned int timeout)
{
	return ws73_usb_transfer(context, (void *)data, size, timeout, false);
}

static int ws73_usb_receive(void *context, u8 *data, size_t size, unsigned int timeout)
{
	return ws73_usb_transfer(context, data, size, timeout, true);
}

static void ws73_usb_initialize(struct work_struct *work)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, initialize);
	struct ws73_chip_observation observation;
	const struct ws73_boot_io io = {
		.context = ws,
		.now_ms = ws73_usb_now,
		.cancelled = ws73_usb_cancelled,
		.send = ws73_usb_send,
		.receive = ws73_usb_receive,
	};
	int ret;

	ret = ws73_boot_read_chip(&io, ws73_usb_now(ws) + 4000, &observation);
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

	/* Match the SDK boot endpoint roles as well as the interface protocol. */
	if (interface->num_altsetting != 1 || alt->desc.bInterfaceNumber ||
	    alt->desc.bAlternateSetting || alt->desc.bNumEndpoints != 2 ||
	    alt->desc.bInterfaceClass != 0xe0 || alt->desc.bInterfaceSubClass != 0x02 ||
	    alt->desc.bInterfaceProtocol != 0x02 ||
	    !usb_endpoint_is_bulk_in(&alt->endpoint[0].desc) ||
	    !usb_endpoint_is_bulk_out(&alt->endpoint[1].desc))
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
	ws->in = alt->endpoint[0].desc.bEndpointAddress;
	ws->out = alt->endpoint[1].desc.bEndpointAddress;
	spin_lock_init(&ws->lock);
	init_usb_anchor(&ws->submitted);
	INIT_WORK(&ws->initialize, ws73_usb_initialize);
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
	usb_kill_anchored_urbs(&ws->submitted);
	cancel_work_sync(&ws->initialize);
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

MODULE_DESCRIPTION("WS73 USB boot identity observation");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
