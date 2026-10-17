// SPDX-License-Identifier: GPL-2.0-only
/* Admission/profile tests inside an empty tagged VM; never register USB. */
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include "ble-profile.h"

static int __init ws73_ble_profile_test_init(void)
{
	const struct firmware *fw;
	struct ws73_customization parsed, unchanged;
	u8 data[141];
	unsigned int i;
	int ret;

	if (!dmi_match(DMI_SYS_VENDOR, "WS73-VM") ||
	    !dmi_match(DMI_PRODUCT_NAME, "WS73-U-trial"))
		return -ENODEV;
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
