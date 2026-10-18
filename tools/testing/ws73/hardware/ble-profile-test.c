// SPDX-License-Identifier: GPL-2.0-only
/* Admission/profile tests inside an empty tagged VM; never register USB. */
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include "ble-profile.h"

static int ws73_ble_identity_test(void)
{
	static const struct {
		int expected;
		u8 package, bus;
		bool valid;
	} cases[] = {
		{ 0, 0, 0, true }, { 2, 2, 0, true },
		{ 0, 2, 0, false }, { 2, 0, 0, false },
		{ 0, 0, 1, false }, { 2, 2, 2, false },
		{ -1, 0, 0, false }, { 1, 1, 0, false },
		{ 3, 3, 0, false }, { 2, 3, 0, false },
	};
	const struct ws73_version known = { .firmware = 0x010a6e3c, .rom = 0x00010101 };
	struct btws73_chip_identity chip = {};
	struct ws73_version changed;
	unsigned int i, bit;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		chip.package = cases[i].package;
		chip.bus = cases[i].bus;
		if (ws73_ble_trial_identity(cases[i].expected, &chip, &known) != cases[i].valid)
			return -EINVAL;
	}
	chip.package = 0;
	chip.bus = 0;
	if (ws73_ble_trial_identity(0, NULL, &known) ||
	    ws73_ble_trial_identity(0, &chip, NULL))
		return -EINVAL;
	chip.bus = 0;
	for (i = 0; i <= 2; i += 2) {
		chip.package = i;
		for (bit = 0; bit < 32; bit++) {
			changed = known;
			changed.firmware ^= 1U << bit;
			if (ws73_ble_trial_identity(i, &chip, &changed))
				return -EINVAL;
			changed = known;
			changed.rom ^= 1U << bit;
			if (ws73_ble_trial_identity(i, &chip, &changed))
				return -EINVAL;
		}
	}
	pr_info("BLE trial identity checks passed: 140\n");
	return 0;
}

static int __init ws73_ble_profile_test_init(void)
{
	const struct firmware *fw;
	struct ws73_customization parsed, unchanged;
	u8 data[141];
	unsigned int i;
	int ret;

	if (!dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    (!dmi_match(DMI_PRODUCT_NAME, "WS73-U-trial") &&
	     !dmi_match(DMI_PRODUCT_NAME, "WS73-E-trial")))
		return -ENODEV;
	ret = ws73_ble_identity_test();
	if (ret)
		return ret;
	ret = request_firmware_direct(&fw, WS73_BLE_TRIAL_PROFILE, NULL);
	if (ret)
		return ret;
	if (fw->size != 140) {
		ret = -EINVAL;
		goto done;
	}
	memcpy(data + 1, fw->data, 140);
	memset(&unchanged, 0xa5, sizeof(unchanged));
	for (i = 0; i <= 141; i++) {
		if (i == 140)
			continue;
		parsed = unchanged;
		if (ws73_ble_trial_profile(data + 1, i, &parsed) != -EMSGSIZE ||
		    memcmp(&parsed, &unchanged, sizeof(parsed))) {
			ret = -EINVAL;
			goto done;
		}
	}
	for (i = 0; i < 140; i++) {
		parsed = unchanged;
		data[i + 1] ^= 1;
		ret = ws73_ble_trial_profile(data + 1, 140, &parsed);
		data[i + 1] ^= 1;
		if (ret != -EBADMSG || memcmp(&parsed, &unchanged, sizeof(parsed))) {
			ret = -EINVAL;
			goto done;
		}
	}
	ret = ws73_ble_trial_profile(data + 1, 140, &parsed);
	if (ret || parsed.coexistence != 1 || parsed.calibration_count != 8 ||
	    parsed.calibration[0].frequency != 6 || parsed.calibration[7].frequency != 76 ||
	    ws73_ble_trial_profile(NULL, 140, &parsed) != -EINVAL ||
	    ws73_ble_trial_profile(data + 1, 140, NULL) != -EINVAL) {
		ret = -EINVAL;
		goto done;
	}
	pr_info("BLE trial profile checks passed: 141 sizes 140 corruptions 2 nulls 1 positive\n");
	ret = 0;
done:
	release_firmware(fw);
	return ret;
}
module_init(ws73_ble_profile_test_init);
static void __exit ws73_ble_profile_test_exit(void) {}
module_exit(ws73_ble_profile_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WS73 trial profile admission checks, no USB access");
MODULE_IMPORT_NS("WS73");
