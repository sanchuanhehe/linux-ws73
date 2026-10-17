// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/module.h>

#include "boot-loader.h"

int ws73_boot_start(struct device *dev, const struct ws73_boot_io *io,
		    const struct ws73_boot_board *board, u64 deadline_ms,
		    struct ws73_boot_session *session)
{
	struct ws73_firmware files = {};
	struct ws73_boot_board selected;
	int ret;

	if (!session)
		return -EINVAL;
	if (session->error || session->stage != WS73_SESSION_NEW)
		return session->error ?: -EALREADY;
	if (!dev || !board || !io || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive || !io->wait_ready) {
		ret = -EINVAL;
		goto fail;
	}
	selected = *board;
	if (selected.xo_coarse > 15 || selected.xo_fine > 127) {
		ret = -ERANGE;
		goto fail;
	}
	session->stage = WS73_SESSION_IDENTITY;
	ret = ws73_boot_read_chip(io, deadline_ms, &session->observation);
	if (ret)
		goto fail;
	session->stage = WS73_SESSION_INPUTS;
	ret = ws73_firmware_request(dev, &session->observation.chip, &files);
	if (ret)
		goto fail;
	/* Never start writes after time spent acquiring/validating the files. */
	if (io->cancelled(io->context)) {
		ret = -ENODEV;
		goto fail;
	}
	if (io->now_ms(io->context) >= deadline_ms) {
		ret = -ETIMEDOUT;
		goto fail;
	}
	session->stage = WS73_SESSION_TRANSFER;
	ret = ws73_boot_load(io, files.image, &selected, deadline_ms, &session->progress);
	if (ret)
		goto fail;
	ws73_firmware_release(&files);
	session->stage = WS73_SESSION_WAIT_RUNTIME;
	return 0;

fail:
	ws73_firmware_release(&files);
	session->error = ret;
	return ret;
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_start, "WS73");
