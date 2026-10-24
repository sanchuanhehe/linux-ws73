// SPDX-License-Identifier: GPL-2.0-only
/* Pure admission checks inside the tagged VM, no USB registration or I/O. */
#include <linux/dmi.h>
#include <linux/module.h>

#include "guard.h"
#include "reset-probe-test.h"

static int __init ws73_trial_guard_test_init(void)
{
	atomic_t available = ATOMIC_INIT(1);
	const struct {
		unsigned int expected;
		unsigned int observed;
		bool init_context;
		int result;
	} cases[] = {
		{ 2, 2, false, -ENODEV }, /* Automatic probe cannot consume admission. */
		{ 2, 3, true, -ENODEV },  /* Another enumeration cannot consume it. */
		{ 0, 0, true, -ENODEV },
		{ 128, 128, true, -ENODEV },
		{ 2, 2, true, 0 },
		{ 2, 2, true, -EALREADY }, /* Includes a failed first bind. */
		{ 2, 3, true, -ENODEV },
		{ 2, 2, true, -EALREADY }, /* Re-enumeration cannot replenish it. */
	};
	const struct {
		int package;
		const char *product;
		bool allowed;
	} profiles[] = {
		{ 0, "WS73-U-trial", true }, { 2, "WS73-E-trial", true },
		{ 0, "WS73-E-trial", false }, { 2, "WS73-U-trial", false },
		{ -1, "WS73-U-trial", false }, { 1, "WS73-E-trial", false },
		{ 0, NULL, false }, { 2, "WS73-trial", false },
	};
	unsigned int i;
	int ret;

	if (!dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    (!dmi_match(DMI_PRODUCT_NAME, "WS73-U-trial") &&
	     !dmi_match(DMI_PRODUCT_NAME, "WS73-E-trial")))
		return -ENODEV;
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		if (ws73_trial_claim(&available, cases[i].expected, cases[i].observed,
				     cases[i].init_context) != cases[i].result) {
			pr_err("RAM trial admission check %u failed\n", i);
			return -EINVAL;
		}
	}
	for (i = 0; i < ARRAY_SIZE(profiles); i++) {
		if (ws73_trial_package_allowed(profiles[i].package, profiles[i].product) !=
		    profiles[i].allowed) {
			pr_err("RAM trial profile check %u failed\n", i);
			return -EINVAL;
		}
	}
	pr_info("RAM trial profile checks passed: %zu\n", ARRAY_SIZE(profiles));
	pr_info("RAM trial admission checks passed: %zu\n", ARRAY_SIZE(cases));
	ret = ws73_reset_probe_checks();
	return ret ? ret : ws73_reset_attempt_checks();
}
module_init(ws73_trial_guard_test_init);

static void __exit ws73_trial_guard_test_exit(void)
{
}
module_exit(ws73_trial_guard_test_exit);

MODULE_DESCRIPTION("WS73 tagged-VM pure one-shot admission checks");
MODULE_LICENSE("GPL");
