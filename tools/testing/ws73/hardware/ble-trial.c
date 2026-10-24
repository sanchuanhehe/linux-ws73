// SPDX-License-Identifier: GPL-2.0-only
/* Explicit one-device VM trial; optional native HCI, no radio test requests. */
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/sched.h>

#include "usb-owner.h"
#include "guard.h"
#include "ble-profile.h"
#include "reset-probe.h"

static char *serial;
module_param(serial, charp, 0400);
static unsigned int devnum;
module_param(devnum, uint, 0400);
static int package = -1;
module_param(package, int, 0400);
/* Only the continuous fresh-load supervisor may request this experiment. */
static bool native_hci;
module_param(native_hci, bool, 0400);
/* Fixed register READ only; mutually exclusive with any function startup. */
static bool inspect_reset;
module_param(inspect_reset, bool, 0400);
/* Explicit controller-closed experiment, not automatic fault recovery. */
static bool device_reset;
module_param(device_reset, bool, 0400);
/* Stop-only on a fresh owner; no startup, reset or consumer publication. */
static bool quiesce_ble;
module_param(quiesce_ble, bool, 0400);
static atomic_t available = ATOMIC_INIT(1);
static struct ws73_customization parameters;

struct ws73_reset_probe {
	struct ws73_usb *ws;
	struct ws73_usb_io io;
};

static u64 ws73_reset_now(void *context)
{
	struct ws73_reset_probe *probe = context;

	return probe->ws->runtime_io.now_ms(probe->ws);
}

static bool ws73_reset_cancelled(void *context)
{
	struct ws73_reset_probe *probe = context;

	return probe->ws->runtime_io.cancelled(probe->ws);
}

static int ws73_reset_send(void *context, const u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_reset_probe *probe = context;

	return ws73_usb_io_transfer(&probe->io, (void *)data, length, timeout, false, false);
}

static int ws73_reset_receive(void *context, u8 *data, size_t length, unsigned int timeout)
{
	struct ws73_reset_probe *probe = context;

	return ws73_usb_io_transfer(&probe->io, data, length, timeout, true, false);
}

static int ws73_reset_observe(struct ws73_usb *ws)
{
	struct usb_host_interface *alt = ws->interface->cur_altsetting;
	struct ws73_reset_probe probe = { .ws = ws, .io = ws->io };
	const struct ws73_boot_io io = {
		.context = &probe,
		.now_ms = ws73_reset_now,
		.cancelled = ws73_reset_cancelled,
		.send = ws73_reset_send,
		.receive = ws73_reset_receive,
	};
	u32 value;
	struct ws73_trial_reset_progress progress;
	int ret;

	if (!ws->runtime || alt->desc.bNumEndpoints != 5 ||
	    alt->endpoint[3].desc.bEndpointAddress != 0x02 ||
	    alt->endpoint[4].desc.bEndpointAddress != 0x82)
		return -ENODEV;
	probe.io.out = 0x02;
	probe.io.in = 0x82;
	if (device_reset) {
		/* This initializer never starts or publishes a function. The host
		 * must separately verify the previous controller's close evidence.
		 */
		if (READ_ONCE(ws->startup.state) != WS73_STARTUP_UNUSED || ws->ble.child)
			return -EBUSY;
		ret = ws73_trial_reset_attempt(&io, ws73_reset_now(&probe) + 6000, &progress);
		dev_info(&ws->interface->dev,
			 "RESET attempt result=%d stage=%u read=%u before=%08x data_attempted=%u transfer=%d; outcome unverified\n",
			 ret, progress.stage, progress.read_valid, progress.before,
			 progress.data_attempted, progress.transfer_result);
		/* The old instance cannot resume, even if the USB write succeeded. */
		ws73_usb_fail(ws, ret ? ret : -ESHUTDOWN);
		return ret;
	}
	ret = ws73_trial_reset_read(&io, ws73_reset_now(&probe) + 4000, &value);
	if (!ret)
		dev_info(&ws->interface->dev,
			 "RESET register observation %#010x; read-only, not a reset\n", value);
	return ret;
}

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

	ret = inspect_reset || device_reset || quiesce_ble ?
		0 : ws73_startup_arm(&ws->startup, now, now + 12000);
	if (!ret)
		ret = ws73_usb_start_rx(ws);
	if (!ret)
		ret = ws73_platform_get_chip(&ws->identity, &ws->tx, &ws->runtime_io,
					     now + 4000, &chip);
	if (!ret)
		ret = ws73_platform_get_version(&ws->identity, &ws->tx, &ws->runtime_io,
						now + 4000, &version);
	if (!ret && !ws73_ble_trial_identity(package, &chip, &version))
		ret = -ENODEV;
	if (ret)
		goto done;
	if (quiesce_ble) {
		stage = "quiesce";
		if (READ_ONCE(ws->startup.state) != WS73_STARTUP_UNUSED || ws->ble.child)
			ret = -EBUSY;
		else
			ret = ws73_ble_control_quiesce(&ws->startup.ble, &ws->ble_io,
						      ws->runtime_io.now_ms(ws) + 2000);
		/* Even an acknowledged stop cannot make this owner usable. */
		ws73_usb_fail(ws, ret ? ret : -ESHUTDOWN);
		goto done;
	}
	if (inspect_reset || device_reset) {
		stage = device_reset ? "device-reset" : "reset-read";
		ret = ws73_reset_observe(ws);
		goto done;
	}
	dev_info(&ws->interface->dev, "BLE trial package %d/USB version matched; reference profile only\n",
		 package);
	stage = "startup";
	ret = ws73_startup_run(&ws->startup, &ws->tx, &ws->runtime_io, &ws->ble_io,
			       &platform, &parameters);
	if (ret)
		goto done;
	stage = "disable";
	ret = ws73_ble_control_set(&ws->startup.ble, &ws->ble_io, false,
				   ws->runtime_io.now_ms(ws) + 2000);
	if (!ret && native_hci) {
		/* No HCI was sent in this fresh-load epoch. The acknowledged close
		 * precedes publication; native open owns the next enable transaction.
		 * This reference-board experiment does not qualify address provenance.
		 */
		stage = "publish";
		ret = ws73_ble_link_publish(&ws->ble, &ws->interface->dev);
	}
done:
	if (ret)
		ws73_usb_fail(ws, ret);
	if (quiesce_ble)
		dev_info(&ws->interface->dev,
			 "QUIESCE trial result=%d stage=%s; stop ACK only, owner terminal\n",
			 ret, stage);
	else if (device_reset)
		dev_info(&ws->interface->dev,
			 "RESET device trial result=%d stage=%s; boot verification required\n",
			 ret, stage);
	else if (inspect_reset)
		dev_info(&ws->interface->dev,
			 "RESET read-only trial result=%d stage=%s\n", ret, stage);
	else if (native_hci)
		dev_info(&ws->interface->dev,
			 "BLE trial result=%d stage=%s; native HCI experiment, address source unqualified\n",
			 ret, stage);
	else
		dev_info(&ws->interface->dev, "BLE trial result=%d stage=%s; no HCI published\n",
			 ret, stage);
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
	    (inspect_reset + native_hci + device_reset + quiesce_ble > 1) ||
	    (native_hci && !IS_ENABLED(CONFIG_BT_WS73)) ||
	    !dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    !ws73_trial_package_allowed(package, dmi_get_system_info(DMI_PRODUCT_NAME)))
		return -ENODEV;
	if (inspect_reset || device_reset || quiesce_ble)
		return usb_register(&ws73_ble_trial_driver);
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
MODULE_DESCRIPTION("WS73 selected-board reference BLE startup trial, tagged VM only");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("WS73");
MODULE_IMPORT_NS("WS73_USB_OWNER");
MODULE_IMPORT_NS("WS73_USB_IO");
