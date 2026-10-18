/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_PLATFORM_CONFIG_H
#define _WS73_PLATFORM_CONFIG_H

#include "usb-tx.h"

/* Explicit board wiring; device sleep remains disabled until PM is supported. */
struct ws73_platform_config {
	u16 wake_gpio;
	bool wake_high;
	bool usb_link_check;
};

/* No defaults or board validation. Output unchanged on error; no overlap. */
int ws73_platform_config_encode(u8 *out, size_t capacity,
				const struct ws73_platform_config *config);
/* Sleepable, bounded by deadline. Success is local TX, not a device ACK. */
int ws73_platform_configure(struct ws73_usb_tx *tx, const struct ws73_usb_tx_io *io,
			    const struct ws73_platform_config *config, u64 deadline);

#endif
