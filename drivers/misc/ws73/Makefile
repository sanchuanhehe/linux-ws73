# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_WS73_CORE) += ws73-core.o
ws73-core-y := hcc.o firmware.o boot.o usb-wire.o usb-tx.o runtime.o service.o ble-control.o
ws73-core-y += customize.o customize-request.o
ws73-core-y += platform-config.o
ws73-core-y += startup.o ble.o ble-link.o
ws73-core-y += boot-loader.o
obj-$(CONFIG_WS73_KUNIT_TEST) += ws73-test.o
ws73-test-y := test.o usb-wire-test.o usb-tx-test.o runtime-test.o service-test.o ble-control-test.o
ws73-test-y += customize-test.o customize-request-test.o
ws73-test-y += platform-config-test.o startup-test.o
ws73-test-y += boot-loader-test.o
ws73-test-y += board-test.o
obj-$(CONFIG_WS73_BLE_MOCK_KUNIT_TEST) += ws73-ble-mock-test.o
ws73-ble-mock-test-y := ble-test.o ble-link-test.o
obj-$(CONFIG_WS73_USB_IO) += ws73-usb-io.o
ws73-usb-io-y := usb-io.o
obj-$(CONFIG_WS73_USB) += ws73-usb.o
ws73-usb-y := usb.o

obj-$(CONFIG_WS73_HCI_KUNIT_TEST) += ws73-hci-test.o
ws73-hci-test-y := ble-hci-test.o ble-hci-data-test.o

obj-$(CONFIG_WS73_USB_BOOT_TEST) += ws73-usb-boot-test.o
ws73-usb-boot-test-y := usb-boot-test.o

obj-$(CONFIG_WS73_USB_STARTUP_TEST) += ws73-usb-startup-test.o
ws73-usb-startup-test-y := usb-startup-test.o
