// SPDX-License-Identifier: GPL-2.0-only
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "boot.h"
#include "usb-wire.h"
#include "usb-io.h"

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

int ws73_usb_io_transfer(const struct ws73_usb_io *io, void *data, size_t size,
			 unsigned int timeout_ms, bool receive, bool control)
{
	struct ws73_usb_request request;
	struct usb_ctrlrequest *setup = NULL;
	size_t limit = io->runtime ? WS73_USB_TX_MAX :
		       receive ? 512 : WS73_BOOT_CHUNK_SIZE;
	struct urb *urb;
	unsigned long flags;
	u8 *buffer;
	int ret;

	if (!data || !size || size > limit ||
	    !timeout_ms || timeout_ms > 2000)
		return -EINVAL;
	if (control && (!io->runtime || receive || size != 4))
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
		usb_fill_control_urb(urb, io->udev, usb_sndctrlpipe(io->udev, 0),
				     (u8 *)setup, buffer, size, ws73_usb_complete, &request);
	} else {
		usb_fill_bulk_urb(urb, io->udev,
				  receive ? usb_rcvbulkpipe(io->udev, io->in) :
				 usb_sndbulkpipe(io->udev, io->out),
				 buffer, size, ws73_usb_complete, &request);
		if (io->runtime && !receive)
			urb->transfer_flags |= URB_ZERO_PACKET;
	}

	spin_lock_irqsave(io->lock, flags);
	if (*io->disconnected || *io->error) {
		ret = *io->disconnected ? -ENODEV : *io->error;
	} else {
		usb_anchor_urb(urb, io->submitted);
		ret = usb_submit_urb(urb, GFP_ATOMIC);
		if (ret)
			usb_unanchor_urb(urb);
	}
	spin_unlock_irqrestore(io->lock, flags);
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
EXPORT_SYMBOL_NS_GPL(ws73_usb_io_transfer, "WS73_USB_IO");

void ws73_usb_io_wait_ready(void *context)
{
	usleep_range(5000, 6000);
}
EXPORT_SYMBOL_NS_GPL(ws73_usb_io_wait_ready, "WS73_USB_IO");

MODULE_DESCRIPTION("WS73 bounded synchronous USB transfers");
MODULE_LICENSE("GPL");
