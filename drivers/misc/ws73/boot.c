// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "boot.h"

#define WS73_BOOT_IO_TIMEOUT_MS 2000

static int ws73_boot_remaining(const struct ws73_boot_io *io, u64 deadline)
{
	u64 now;

	if (io->cancelled(io->context))
		return -ENODEV;
	now = io->now_ms(io->context);
	if (now >= deadline)
		return -ETIMEDOUT;
	return min_t(u64, deadline - now, WS73_BOOT_IO_TIMEOUT_MS);
}

static int ws73_boot_send(const struct ws73_boot_io *io, const u8 *data,
			  size_t len, u64 deadline)
{
	int timeout = ws73_boot_remaining(io, deadline);
	int ret;

	if (timeout < 0)
		return timeout;
	ret = io->send(io->context, data, len, timeout);
	if (ret < 0)
		return ret;
	if (ret != len)
		return -EREMOTEIO;
	ret = ws73_boot_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

static int ws73_boot_expect(const struct ws73_boot_io *io, const char *expected,
			    u64 deadline)
{
	u8 reply[512];
	size_t len = strlen(expected);
	int timeout = ws73_boot_remaining(io, deadline);
	int ret;

	if (timeout < 0)
		return timeout;
	ret = io->receive(io->context, reply, sizeof(reply), timeout);
	if (ret < 0)
		return ret;
	if (ret < len || ret > sizeof(reply))
		return -EMSGSIZE;
	/* Permit NUL padding, not an error string sharing a success prefix. */
	if (memcmp(reply, expected, len) || memchr_inv(reply + len, 0, ret - len))
		return -EPROTO;
	ret = ws73_boot_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

int ws73_boot_download(const struct ws73_boot_io *io,
		       const struct ws73_fw_view image[WS73_FW_COUNT],
		       u64 deadline_ms, struct ws73_boot_progress *progress)
{
	static const u32 addresses[WS73_FW_COUNT] = { 0x400000, 0x430000, 0x440000 };
	char command[80];
	unsigned int i;
	int ret;

	if (!io || !image || !progress || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive || !io->wait_ready)
		return -EINVAL;
	/* Check every range before the first side effect, including overlap. */
	for (i = 0; i < WS73_FW_COUNT; i++) {
		if (!image[i].data || !image[i].size ||
		    image[i].size > WS73_FW_MAX_SIZE - WS73_FW_HASH_HEADER ||
		    image[i].address != addresses[i] ||
		    (i + 1 < WS73_FW_COUNT && image[i].size > addresses[i + 1] - addresses[i]))
			return -EINVAL;
	}

	for (i = 0; i < WS73_FW_COUNT; i++) {
		size_t offset = 0;

		progress->image = i;
		progress->acknowledged = 0;
		while (offset < image[i].size) {
			size_t len = min_t(size_t, WS73_BOOT_CHUNK_SIZE, image[i].size - offset);
			bool last = len == image[i].size - offset;
			unsigned int flag;
			int command_len;

			/* SDK verification flags: start=0, middle=1, end=2, single=3. */
			flag = !offset ? (last ? 3 : 0) : (last ? 2 : 1);
			command_len = scnprintf(command, sizeof(command),
						"FILES 1 0x%x 0x%zx 0x%x ",
						image[i].address + (u32)offset, len, flag);
			progress->stage = WS73_BOOT_COMMAND;
			ret = ws73_boot_send(io, command, command_len, deadline_ms);
			if (ret)
				return ret;
			progress->stage = WS73_BOOT_READY;
			ret = ws73_boot_expect(io, "READY", deadline_ms);
			if (ret)
				return ret;
			io->wait_ready(io->context);
			progress->stage = WS73_BOOT_PAYLOAD;
			ret = ws73_boot_send(io, image[i].data + offset, len, deadline_ms);
			if (ret)
				return ret;
			progress->stage = WS73_BOOT_FILES_OK;
			ret = ws73_boot_expect(io, "FILES OK", deadline_ms);
			if (ret)
				return ret;
			offset += len;
			progress->acknowledged = offset;
		}
	}
	progress->stage = WS73_BOOT_QUIT;
	ret = ws73_boot_send(io, "QUIT", 4, deadline_ms);
	if (ret)
		return ret;
	/* The USB SDK has an EMPTY QUIT response; runtime probe is the gate. */
	progress->stage = WS73_BOOT_WAIT_RUNTIME;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_download, "WS73");
