// SPDX-License-Identifier: GPL-2.0-only
/* Hardware-free download executor. Never bind a physical USB controller. */
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "boot.h"
#include "boot-loader.h"
#include "usb-io.h"

struct ws73_download_test {
	struct usb_interface *intf;
	struct usb_anchor submitted;
	spinlock_t lock; /* Admission and disconnect state shared with USB IO. */
	bool disconnected;
	bool loader;
	int error;
	struct work_struct work;
	struct ws73_usb_io usb;
	u8 *payload;
};

static u64 ws73_test_now(void *context)
{
	return ktime_to_ms(ktime_get());
}

static bool ws73_test_cancelled(void *context)
{
	struct ws73_download_test *test = context;

	return READ_ONCE(test->disconnected);
}

static int ws73_test_send(void *context, const u8 *data, size_t len,
			  unsigned int timeout_ms)
{
	struct ws73_download_test *test = context;

	if (len == WS73_BOOT_CHUNK_SIZE)
		dev_info(&test->intf->dev, "synthetic payload admission\n");
	return ws73_usb_io_transfer(&test->usb, (void *)data, len,
				    timeout_ms, false, false);
}

static int ws73_test_receive(void *context, u8 *data, size_t len,
			     unsigned int timeout_ms)
{
	struct ws73_download_test *test = context;

	return ws73_usb_io_transfer(&test->usb, data, len, timeout_ms, true, false);
}

static void ws73_test_wait_ready(void *context)
{
	struct ws73_download_test *test = context;
	u64 start = ktime_get_ns(), elapsed;
	unsigned long flags;

	ws73_usb_io_wait_ready(context);
	elapsed = ktime_get_ns() - start;
	/* USB scheduling can mask a missing sleep in the gadget's wire timing. */
	if (elapsed < 5000000) {
		spin_lock_irqsave(&test->lock, flags);
		test->error = -ETIME;
		spin_unlock_irqrestore(&test->lock, flags);
	}
	dev_info(&test->intf->dev, "synthetic READY wait ns=%llu\n", elapsed);
}

static void ws73_test_download(struct work_struct *work)
{
	struct ws73_download_test *test = container_of(work, typeof(*test), work);
	struct ws73_boot_io io = {
		.context = test,
		.now_ms = ws73_test_now,
		.cancelled = ws73_test_cancelled,
		.send = ws73_test_send,
		.receive = ws73_test_receive,
		.wait_ready = ws73_test_wait_ready,
	};
	struct ws73_fw_view images[] = {
		{ test->payload, 32769, 0x400000 },
		{ test->payload + 32769, 32, 0x430000 },
		{ test->payload + 32801, 32, 0x440000 },
	};
	struct ws73_boot_progress progress = {};
	unsigned int i, j;
	int ret;

	if (test->loader) {
		struct ws73_boot_session session = {};
		/* Explicit fixture trim, not a default for any physical board. */
		const struct ws73_boot_board board = { .xo_coarse = 1, .xo_fine = 2 };
		/* Explicit test input from dummy_hcd, never a physical USB identity. */
		const struct btws73_chip_identity expected = {
			.package = le16_to_cpu(test->usb.udev->descriptor.bcdDevice),
			.bus = 0,
		};
		u64 deadline = ws73_test_now(test) + 15000;
		int repeated;

		ret = ws73_boot_start(&test->intf->dev, &io, &board, &expected,
				      deadline, &session);
		repeated = ws73_boot_start(&test->intf->dev, &io, &board, &expected,
					   deadline, &session);
		dev_info(&test->intf->dev,
			 "synthetic loader result=%d repeat=%d phase=%u stage=%u prep=%u image=%u ack=%zu raw=%08x\n",
			 ret, repeated, session.stage, session.progress.stage,
			 session.progress.preparation_writes, session.progress.image,
			 session.progress.acknowledged, session.observation.raw);
		return;
	}

	/* Explicit synthetic views, not original firmware or qualified boards.
	 * Hash validation and RAM/XO preparation have separate tests. Exercise
	 * the production download sequencer and actual USB lifetime here.
	 */
	for (i = 0; i < ARRAY_SIZE(images); i++)
		for (j = 0; j < images[i].size; j++)
			((u8 *)images[i].data)[j] = j * 37 + 11 + i * 19;
	ret = ws73_boot_download(&io, images, ws73_test_now(test) + 10000, &progress);
	dev_info(&test->intf->dev, "synthetic download result=%d stage=%u image=%u ack=%zu\n",
		 ret, progress.stage, progress.image, progress.acknowledged);
}

static int ws73_test_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usb_host_interface *alt = intf->cur_altsetting;
	struct ws73_download_test *test;
	struct device *controller = udev->bus->controller;

	/* The synthetic ID alone must never authorize writes to real hardware. */
	if (!controller->driver || strcmp(controller->driver->name, "dummy_hcd") ||
	    udev->descriptor.bNumConfigurations != 1 || intf->num_altsetting != 1 ||
	    alt->desc.bInterfaceNumber || alt->desc.bAlternateSetting ||
	    alt->desc.bInterfaceClass != 0xe0 || alt->desc.bInterfaceSubClass != 2 ||
	    alt->desc.bInterfaceProtocol != 2 || alt->desc.bNumEndpoints != 2 ||
	    !usb_endpoint_is_bulk_in(&alt->endpoint[0].desc) ||
	    !usb_endpoint_is_bulk_out(&alt->endpoint[1].desc))
		return -ENODEV;
	test = kzalloc_obj(*test);
	if (!test)
		return -ENOMEM;
	test->payload = kmalloc(32833, GFP_KERNEL);
	if (!test->payload) {
		kfree(test);
		return -ENOMEM;
	}
	test->intf = intf;
	test->loader = id->driver_info;
	spin_lock_init(&test->lock);
	init_usb_anchor(&test->submitted);
	INIT_WORK(&test->work, ws73_test_download);
	test->usb = (struct ws73_usb_io) {
		.udev = usb_get_dev(udev),
		.submitted = &test->submitted,
		.lock = &test->lock,
		.disconnected = &test->disconnected,
		.error = &test->error,
		.in = usb_endpoint_num(&alt->endpoint[0].desc),
		.out = usb_endpoint_num(&alt->endpoint[1].desc),
	};
	usb_set_intfdata(intf, test);
	schedule_work(&test->work);
	return 0;
}

static void ws73_test_disconnect(struct usb_interface *intf)
{
	struct ws73_download_test *test = usb_get_intfdata(intf);
	unsigned long flags;

	usb_set_intfdata(intf, NULL);
	spin_lock_irqsave(&test->lock, flags);
	test->disconnected = true;
	spin_unlock_irqrestore(&test->lock, flags);
	usb_kill_anchored_urbs(&test->submitted);
	cancel_work_sync(&test->work);
	usb_put_dev(test->usb.udev);
	kfree(test->payload);
	kfree(test);
	dev_info(&intf->dev, "synthetic download callbacks drained\n");
}

static const struct usb_device_id ws73_test_ids[] = {
	{ USB_DEVICE(0xffff, 0x3734) },
	{ USB_DEVICE(0xffff, 0x3735), .driver_info = 1 },
	{ }
};
MODULE_DEVICE_TABLE(usb, ws73_test_ids);

static struct usb_driver ws73_test_driver = {
	.name = "ws73-download-test",
	.id_table = ws73_test_ids,
	.probe = ws73_test_probe,
	.disconnect = ws73_test_disconnect,
};
module_usb_driver(ws73_test_driver);

MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("WS73_USB_IO");
MODULE_DESCRIPTION("WS73 synthetic USB boot download test");
MODULE_LICENSE("GPL");
