/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_USB_IO_H
#define _WS73_USB_IO_H

#include <linux/usb.h>

/* Borrowed owner state. All pointers and the device remain live until callers
 * and completions are joined. Admission shares the owner's state lock; the
 * owner closes admission and kills anchored requests to interrupt teardown.
 * The helper additionally joins each completion after unanchoring.
 */
struct ws73_usb_io {
	struct usb_device *udev;
	struct usb_anchor *submitted;
	spinlock_t *lock; /* Owner admission/error state. */
	const bool *disconnected;
	const int *error;
	bool runtime;
	u8 in;
	u8 out;
};

int ws73_usb_io_transfer(const struct ws73_usb_io *io, void *data, size_t size,
			 unsigned int timeout_ms, bool receive, bool control);
void ws73_usb_io_wait_ready(void *context);

#endif
