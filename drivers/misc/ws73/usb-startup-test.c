// SPDX-License-Identifier: GPL-2.0-only
/* Exercise the production USB owner against a synthetic runtime peer only. */
#include <linux/module.h>

#include "usb-owner.h"

static void ws73_test_initialize_common(struct work_struct *work, bool publish)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, initialize);
	/* Fixture parameters only: no physical board applicability is claimed. */
	const struct ws73_platform_config platform = {};
	const struct ws73_customization parameters = {};
	struct btws73_chip_identity chip;
	struct ws73_version version;
	const char *stage = "identity";
	u64 now = ws->runtime_io.now_ms(ws);
	unsigned long flags;
	unsigned int state, action;
	int ret;

	ret = ws73_startup_arm(&ws->startup, now, now + 12000);
	if (!ret)
		ret = ws73_usb_start_rx(ws);
	if (!ret)
		ret = ws73_platform_get_chip(&ws->identity, &ws->tx, &ws->runtime_io,
					     now + 4000, &chip);
	if (!ret)
		ret = ws73_platform_get_version(&ws->identity, &ws->tx, &ws->runtime_io,
						now + 4000, &version);
	if (ret)
		goto done;
	/* Only the explicit synthetic fixture identity is accepted here. */
	if (chip.package != 0 || chip.bus != 0 || version.firmware != 0x010a6e7f ||
	    version.rom != 0x12345678) {
		ret = -ENODEV;
		goto done;
	}
	dev_info(&ws->interface->dev, "synthetic runtime identity verified\n");
	stage = "startup";
	ret = ws73_startup_run(&ws->startup, &ws->tx, &ws->runtime_io,
			       &ws->ble_io, &platform, &parameters);
	if (ret)
		goto done;
	stage = "disable";
	ret = ws73_ble_control_set(&ws->startup.ble, &ws->ble_io, false,
				   ws->runtime_io.now_ms(ws) + 2000);
done:
	if (ret)
		ws73_usb_fail(ws, ret);
	spin_lock_irqsave(&ws->startup.lock, flags);
	state = ws->startup.state;
	spin_unlock_irqrestore(&ws->startup.lock, flags);
	spin_lock_irqsave(&ws->startup.ble.lock, flags);
	action = ws->startup.ble.state;
	spin_unlock_irqrestore(&ws->startup.ble.lock, flags);
	dev_info(&ws->interface->dev,
		 "synthetic startup result=%d stage=%s state=%u action=%u\n",
		 ret, stage, state, action);
	if (!ret && publish) {
		ret = ws73_ble_link_publish(&ws->ble, &ws->interface->dev);
		if (ret)
			ws73_usb_fail(ws, ret);
		dev_info(&ws->interface->dev, "synthetic auxiliary result=%d\n", ret);
	}
}

static void ws73_test_initialize(struct work_struct *work)
{
	ws73_test_initialize_common(work, false);
}

static void ws73_test_initialize_hci(struct work_struct *work)
{
	ws73_test_initialize_common(work, true);
}

static int ws73_test_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct device *controller = udev->bus->controller;

	/* Neither a spoofed test ID nor a module parameter permits physical I/O. */
	if (!controller->driver || strcmp(controller->driver->name, "dummy_hcd") ||
	    udev->descriptor.bNumConfigurations != 1 ||
	    interface->cur_altsetting->desc.bNumEndpoints != 5)
		return -ENODEV;
	return ws73_usb_bind(interface, id->driver_info ?
			     ws73_test_initialize_hci : ws73_test_initialize);
}

static const struct usb_device_id ws73_test_ids[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0xffff, 0x3736, 0xe0, 0x02, 0x02) },
	{ USB_DEVICE_AND_INTERFACE_INFO(0xffff, 0x3737, 0xe0, 0x02, 0x02),
	  .driver_info = 1 },
	{}
};
MODULE_DEVICE_TABLE(usb, ws73_test_ids);

static struct usb_driver ws73_test_driver = {
	.name = "ws73-startup-test",
	.probe = ws73_test_probe,
	.disconnect = ws73_usb_unbind,
	.id_table = ws73_test_ids,
};
module_usb_driver(ws73_test_driver);

MODULE_DESCRIPTION("WS73 hardware-free USB startup integration test");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("WS73_USB_OWNER");
