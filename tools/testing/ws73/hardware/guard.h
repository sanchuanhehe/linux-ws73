/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_TRIAL_GUARD_H
#define WS73_TRIAL_GUARD_H

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/string.h>

/* Explicit package and tagged VM must agree; neither is a chip query. */
static inline bool ws73_trial_package_allowed(int package, const char *product)
{
	if (!product)
		return false;
	return (package == 0 && !strcmp(product, "WS73-U-trial")) ||
	       (package == 2 && !strcmp(product, "WS73-E-trial"));
}

/* One explicit PID 1 bind of the observed guest enumeration, even on failure. */
static inline int ws73_trial_claim(atomic_t *available, unsigned int expected,
				   unsigned int observed, bool init_context)
{
	if (!init_context || !expected || expected > 127 || observed != expected)
		return -ENODEV;
	return atomic_cmpxchg(available, 1, 0) == 1 ? 0 : -EALREADY;
}

#endif
