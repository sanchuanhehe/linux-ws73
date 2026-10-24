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

enum ws73_trial_reset_stage {
	WS73_RESET_READ,
	WS73_RESET_WRITE_SETUP,
	WS73_RESET_WRITE_DATA,
};

struct ws73_trial_reset_progress {
	enum ws73_trial_reset_stage stage;
	u32 before;
	bool read_valid;
	bool data_attempted;
	int transfer_result;
};

/* A local completed write never proves reset, state clearing or a new epoch.
 * Once data_attempted is set, even an error may mean the device reset during
 * the transfer. Record it and observe a new boot instance; never retry.
 * The caller must have no active consumers and make the old owner terminal.
 */
static inline int ws73_trial_reset_attempt(const struct ws73_boot_io *io, u64 deadline,
					   struct ws73_trial_reset_progress *progress)
{
	static const u8 setup[16] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0x18, 0xf1, 0, 0, 9, 0, 0, 0x40,
	};
	u32 value;
	u8 data[4];
	int ret, timeout;

	if (!progress || !io || !io->now_ms || !io->cancelled || !io->send || !io->receive)
		return -EINVAL;
	*progress = (struct ws73_trial_reset_progress) { .stage = WS73_RESET_READ };
	ret = ws73_trial_reset_read(io, deadline, &value);
	if (ret)
		return ret;
	progress->before = value;
	progress->read_valid = true;
	if (value & BIT(4))
		return -EALREADY;
	timeout = ws73_trial_reset_remaining(io, deadline);
	if (timeout < 0)
		return timeout;
	progress->stage = WS73_RESET_WRITE_SETUP;
	ret = io->send(io->context, setup, sizeof(setup), timeout);
	if (ret < 0)
		return ret;
	if (ret != sizeof(setup))
		return ret > sizeof(setup) ? -EOVERFLOW : -EREMOTEIO;
	timeout = ws73_trial_reset_remaining(io, deadline);
	if (timeout < 0)
		return timeout;
	put_unaligned_le32(value | BIT(4), data);
	progress->stage = WS73_RESET_WRITE_DATA;
	progress->data_attempted = true;
	ret = io->send(io->context, data, sizeof(data), timeout);
	progress->transfer_result = ret;
	if (ret < 0)
		return ret;
	if (ret != sizeof(data))
		return ret > sizeof(data) ? -EOVERFLOW : -EREMOTEIO;
	ret = ws73_trial_reset_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

#endif
