# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_WS73_CORE) += ws73-core.o
ws73-core-y := hcc.o firmware.o boot.o usb-wire.o usb-tx.o runtime.o service.o ble-control.o
ws73-core-y += customize.o customize-request.o
ws73-core-y += startup.o ble.o
obj-$(CONFIG_WS73_KUNIT_TEST) += ws73-test.o
ws73-test-y := test.o usb-wire-test.o usb-tx-test.o runtime-test.o service-test.o ble-control-test.o
ws73-test-y += customize-test.o customize-request-test.o
ws73-test-y += startup-test.o
ws73-test-y += board-test.o ble-test.o
obj-$(CONFIG_WS73_USB) += ws73-usb.o
ws73-usb-y := usb.o
