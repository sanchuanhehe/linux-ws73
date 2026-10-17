/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_USB_OWNER_H
#define _WS73_USB_OWNER_H

#include <linux/usb.h>
#include <linux/workqueue.h>

#include "boot.h"
#include "usb-io.h"
#include "ble-link.h"
#include "startup.h"
#include "runtime.h"

/* Private USB owner state, shared with the dummy_hcd integration consumer. */
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
	struct ws73_usb_tx_io runtime_io;
	struct ws73_ble_link ble;
	struct ws73_boot_io boot_io;
	struct ws73_usb_io io;
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

/* bind queues initialize after all owner resources are ready. */
int ws73_usb_bind(struct usb_interface *interface, work_func_t initialize);
/* Stop admission and join every callback/worker before freeing owner state. */
void ws73_usb_unbind(struct usb_interface *interface);
/* Initialization worker only, once on a runtime interface; failure is terminal. */
int ws73_usb_start_rx(struct ws73_usb *ws);
void ws73_usb_fail(struct ws73_usb *ws, int error);

#endif
