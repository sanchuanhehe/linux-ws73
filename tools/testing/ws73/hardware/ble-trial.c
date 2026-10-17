// SPDX-License-Identifier: GPL-2.0-only
/* Explicit one-device VM trial; no HCI publication or radio traffic commands. */
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/sched.h>

#include "usb-owner.h"
#include "guard.h"
#include "ble-profile.h"

static char *serial;
module_param(serial, charp, 0400);
static unsigned int devnum;
module_param(devnum, uint, 0400);
static atomic_t available = ATOMIC_INIT(1);
static struct ws73_customization parameters;

static void ws73_ble_trial_start(struct work_struct *work)
{
	struct ws73_usb *ws = container_of(work, struct ws73_usb, initialize);
	/* Explicit reference wiring; independent Linux policy keeps sleep off. */
	const struct ws73_platform_config platform = { .wake_gpio = 10, .wake_high = true };
	struct btws73_chip_identity chip;
	struct ws73_version version;
	const char *stage = "identity";
	u64 now = ws->runtime_io.now_ms(ws);
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
	if (!ret && (chip.package || chip.bus || version.firmware != 0x010a6e3c ||
		     version.rom != 0x00010101))
		ret = -ENODEV;
	if (ret)
		goto done;
	dev_info(&ws->interface->dev, "BLE trial U/USB version matched; reference profile only\n");
	stage = "startup";
	ret = ws73_startup_run(&ws->startup, &ws->tx, &ws->runtime_io, &ws->ble_io,
			       &platform, &parameters);
	if (ret)
		goto done;
	stage = "disable";
	ret = ws73_ble_control_set(&ws->startup.ble, &ws->ble_io, false,
				   ws->runtime_io.now_ms(ws) + 2000);
done:
	if (ret)
		ws73_usb_fail(ws, ret);
	dev_info(&ws->interface->dev, "BLE trial result=%d stage=%s; no HCI published\n", ret, stage);
}

static int ws73_ble_trial_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	int ret;

	if (!udev->serial || strcmp(udev->serial, serial) ||
	    strcmp(dev_name(&udev->dev), "1-1") ||
	    intf->cur_altsetting->desc.bNumEndpoints != 5)
		return -ENODEV;
	ret = ws73_trial_claim(&available, devnum, udev->devnum, is_global_init(current));
	if (ret)
		return ret;
	return ws73_usb_bind(intf, ws73_ble_trial_start);
}

static const struct usb_device_id ws73_ble_trial_ids[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0xffff, 0x3733, 0xe0, 0x02, 0x02) },
	{}
};

/* No MODULE_DEVICE_TABLE: explicit PID 1 binding only, never hardware autoload. */
static struct usb_driver ws73_ble_trial_driver = {
	.name = "ws73-ble-trial",
	.probe = ws73_ble_trial_probe,
	.disconnect = ws73_usb_unbind,
	.id_table = ws73_ble_trial_ids,
	.no_dynamic_id = 1,
};

static int __init ws73_ble_trial_init(void)
{
	const struct firmware *fw;
	int ret;

	if (!serial || !*serial || !devnum || devnum > 127 ||
	    !dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    !dmi_match(DMI_PRODUCT_NAME, "WS73-U-trial"))
		return -ENODEV;
	ret = request_firmware_direct(&fw, WS73_BLE_TRIAL_PROFILE, NULL);
	if (ret)
		return ret;
	ret = ws73_ble_trial_profile(fw->data, fw->size, &parameters);
	release_firmware(fw);
	if (ret)
		return ret;
	return usb_register(&ws73_ble_trial_driver);
}
module_init(ws73_ble_trial_init);

static void __exit ws73_ble_trial_exit(void)
{
	usb_deregister(&ws73_ble_trial_driver);
}
module_exit(ws73_ble_trial_exit);
MODULE_DESCRIPTION("WS73 allocated-U reference BLE startup trial, tagged VM only");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("WS73_USB_OWNER");
