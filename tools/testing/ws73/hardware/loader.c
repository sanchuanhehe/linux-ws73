// SPDX-License-Identifier: GPL-2.0-only
/* Explicit, one-device RAM loading trial. Never install/load on the host. */
#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/sched.h>

#include "boot-loader.h"
#include "usb-owner.h"
#include "guard.h"

static char *serial;
module_param(serial, charp, 0400);
MODULE_PARM_DESC(serial, "Exact allocated USB serial, required inside the tagged trial VM");
static unsigned int devnum;
module_param(devnum, uint, 0400);
MODULE_PARM_DESC(devnum, "Guest device number from the successful observation, required");
static atomic_t available = ATOMIC_INIT(1);

static void ws73_trial_load(struct work_struct *work)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, initialize);
	/* Explicit SDK110 USB reference INI input, not a universal board default. */
	const struct ws73_boot_board board = { .xo_coarse = 8, .xo_fine = 60 };
	struct ws73_boot_session session = {};
	int ret;

	ret = ws73_boot_start(&ws->interface->dev, &ws->boot_io, &board,
			      ws->boot_io.now_ms(ws) + 15000, &session);
	if (ret)
		ws73_usb_fail(ws, ret);
	dev_info(&ws->interface->dev,
		 "RAM trial result=%d phase=%u stage=%u prep=%u image=%u ack=%zu raw=%08x\n",
		 ret, session.stage, session.progress.stage,
		 session.progress.preparation_writes, session.progress.image,
		 session.progress.acknowledged, session.observation.raw);
}

static int ws73_trial_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	int ret;

	/* Runtime creates a fresh owner through the normal observation driver. */
	if (!udev->serial || strcmp(udev->serial, serial) ||
	    strcmp(dev_name(&udev->dev), "1-1") ||
	    intf->cur_altsetting->desc.bNumEndpoints != 2)
		return -ENODEV;
	ret = ws73_trial_claim(&available, devnum, udev->devnum, is_global_init(current));
	if (ret)
		return ret;
	/* Consumed before bind: allocation/probe failure must not permit retry. */
	return ws73_usb_bind(intf, ws73_trial_load);
}

static const struct usb_device_id ws73_trial_ids[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0xffff, 0x3733, 0xe0, 0x02, 0x02) },
	{}
};

/* No device alias: this trial must never be loaded by hardware autoloading. */
static struct usb_driver ws73_trial_driver = {
	.name = "ws73-ram-trial",
	.probe = ws73_trial_probe,
	.disconnect = ws73_usb_unbind,
	.id_table = ws73_trial_ids,
	.no_dynamic_id = 1,
};

static int __init ws73_trial_init(void)
{
	if (!serial || !*serial || !devnum || devnum > 127 ||
	    !dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    !dmi_match(DMI_PRODUCT_NAME, "WS73-U-trial"))
		return -ENODEV;
	return usb_register(&ws73_trial_driver);
}
module_init(ws73_trial_init);

static void __exit ws73_trial_exit(void)
{
	usb_deregister(&ws73_trial_driver);
}
module_exit(ws73_trial_exit);

MODULE_DESCRIPTION("Explicit WS73 SDK110 reference RAM loading trial, tagged VM only");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("WS73_USB_OWNER");
