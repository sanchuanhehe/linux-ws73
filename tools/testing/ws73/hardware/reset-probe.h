/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_TRIAL_RESET_PROBE_H
#define WS73_TRIAL_RESET_PROBE_H

#include <linux/unaligned.h>

#include "boot.h"

/* Fixed runtime register-endpoint READ of 0x4001f118. No reset/data write.
 * The caller exclusively owns this register transaction and discards the
 * instance after an error. The boot_io type supplies only the I/O contract;
 * these callbacks must address runtime register endpoints, not boot/data.
 */
static inline int ws73_trial_reset_remaining(const struct ws73_boot_io *io, u64 deadline)
{
	u64 now;

	if (io->cancelled(io->context))
		return -ENODEV;
	now = io->now_ms(io->context);
	if (now >= deadline)
		return -ETIMEDOUT;
	return min_t(u64, deadline - now, 2000);
}

static inline int ws73_trial_reset_read(const struct ws73_boot_io *io,
					u64 deadline, u32 *value)
{
	static const u8 setup[16] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0x18, 0xf1, 0, 0, 8, 0, 0, 0x40,
	};
	u8 data[4];
	int ret, timeout;

	if (!io || !value || !io->now_ms || !io->cancelled || !io->send || !io->receive)
		return -EINVAL;
	timeout = ws73_trial_reset_remaining(io, deadline);
	if (timeout < 0)
		return timeout;
	ret = io->send(io->context, setup, sizeof(setup), timeout);
	if (ret < 0)
		return ret;
	if (ret != sizeof(setup))
		return ret > sizeof(setup) ? -EOVERFLOW : -EREMOTEIO;
	timeout = ws73_trial_reset_remaining(io, deadline);
	if (timeout < 0)
		return timeout;
	ret = io->receive(io->context, data, sizeof(data), timeout);
	if (ret < 0)
		return ret;
	if (ret != sizeof(data))
		return ret > sizeof(data) ? -EOVERFLOW : -EREMOTEIO;
	ret = ws73_trial_reset_remaining(io, deadline);
	if (ret < 0)
		return ret;
	*value = get_unaligned_le32(data);
	return 0;
}

#endif
