// SPDX-License-Identifier: GPL-2.0-only
/* Tagged VM PID 1 only: observe, explicitly rebind one selected U/E, capture and stop. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "usbmon.h"

static void fail(const char *where)
{
 printf("WS73_RAM_TRIAL_ERROR %s errno=%d\n", where, errno);
 reboot(RB_POWER_OFF);
 for (;;) pause();
}
static void read_text(const char *path, char *data, size_t capacity)
{
 int fd = open(path, O_RDONLY);
 if (fd < 0) fail(path);
 ssize_t n = read(fd, data, capacity - 1);
 if (n <= 0 || (size_t)n >= capacity - 1) fail(path);
 data[n] = 0; close(fd);
 data[strcspn(data, "\r\n")] = 0;
}
static void bind_command(const char *path)
{
 static const char interface[] = "1-1:1.0";
 int fd = open(path, O_WRONLY);
 if (fd < 0 || write(fd, interface, sizeof(interface) - 1) != sizeof(interface) - 1) fail(path);
 close(fd);
}
static void load_module(const char *path, const char *parameters, int expected_error)
{
 int fd = open(path, O_RDONLY);
 if (fd < 0) fail(path);
 int ret = syscall(SYS_finit_module, fd, parameters, 0);
 int error = ret < 0 ? errno : 0;
 close(fd);
 if (error != expected_error) { errno = error; fail("module result"); }
}
static void require_observer(void)
{
 if (access("/sys/bus/usb/drivers/ws73/1-1:1.0", F_OK)) fail("observer ownership");
}
int main(void)
{
 if (getpid() != 1) return 2;
 int console = open("/dev/ttyS1", O_RDWR);
 struct termios tty;
 if (console < 0 || tcgetattr(console, &tty)) return 2;
 cfmakeraw(&tty); cfsetospeed(&tty, B115200); cfsetispeed(&tty, B115200);
 if (tcsetattr(console, TCSANOW, &tty)) return 2;
 dup2(console, 0); dup2(console, 1); dup2(console, 2); setvbuf(stdout, NULL, _IONBF, 0);
 if (mount("proc", "/proc", "proc", 0, NULL) || mount("sysfs", "/sys", "sysfs", 0, NULL) ||
     mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL)) fail("mount");
 char serial[128], value[128], parameters[160], board[8];
 unsigned int package;
 const char *product, *identity_line;
 read_text("/trial-board", board, sizeof(board));
 if (!strcmp(board, "us")) {
  package = 0; product = "WS73-U-trial";
  identity_line = "ws73 1-1:1.0: boot chip observation 0x00000000: package 0 bus 0;";
 } else if (!strcmp(board, "e")) {
  package = 2; product = "WS73-E-trial";
  identity_line = "ws73 1-1:1.0: boot chip observation 0x0000000a: package 2 bus 0;";
 } else { fail("explicit board missing or invalid"); }
 read_text("/sys/class/dmi/id/sys_vendor", value, sizeof(value));
 if (strcmp(value, "WS73-VM")) fail("VM vendor guard");
 read_text("/sys/class/dmi/id/product_name", value, sizeof(value));
 if (strcmp(value, product)) fail("VM product guard");
 read_text("/trial-serial", serial, sizeof(serial));
 if (strpbrk(serial, "\"\\ \t")) fail("serial parameter syntax");
 snprintf(parameters, sizeof(parameters), "serial=\"%s\"", serial);
 /* Pure gate checks and an invalid-module-input check, before any attachment. */
 load_module("/guard-test.ko", "", 0);
 if (syscall(SYS_delete_module, "ws73_ram_trial_guard_test", O_NONBLOCK)) fail("guard test unload");
 load_module("/trial.ko", parameters, ENODEV); /* Missing enumeration is forbidden. */
 snprintf(parameters, sizeof(parameters), "serial=\"%s\" devnum=2", serial);
 load_module("/trial.ko", parameters, ENODEV); /* Missing package is forbidden. */
 snprintf(parameters, sizeof(parameters), "serial=\"%s\" devnum=2 package=%u", serial, package == 0 ? 2 : 0);
 load_module("/trial.ko", parameters, ENODEV); /* Wrong tagged package is forbidden. */
 errno = 0;
 if (!access("/sys/bus/usb/drivers/ws73-ram-trial", F_OK) || errno != ENOENT)
  fail("premature trial registration");
 int monitor = open("/sys/kernel/debug/usb/usbmon/0u", O_RDONLY | O_NONBLOCK);
 int kmsg = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
 if (monitor < 0 || kmsg < 0) fail("capture");
 int binary = ws73_mon_open();
 if (binary < 0) fail("binary usbmon open");
 puts("WS73_MON_READY capacity=512 ring=1048576");
 puts("WS73_RAM_TRIAL_GUARDS_PASS_NO_DRIVER");
 puts("WS73_RAM_TRIAL_READY_NO_DEVICE");
 struct timespec start, now;
 clock_gettime(CLOCK_MONOTONIC, &start);
 int identity = 0, attempted = 0;
 unsigned int observed_devnum = 0;
 for (;;) {
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (now.tv_sec - start.tv_sec >= 90) fail("absolute VM deadline");
  if (ws73_mon_drain(binary)) fail("binary usbmon capture");
  struct pollfd fds[] = {{monitor, POLLIN, 0}, {kmsg, POLLIN, 0}};
  if (poll(fds, 2, 100) < 0 && errno != EINTR) fail("capture poll");
  for (unsigned int i = 0; i < 2; i++) {
   for (;;) {
    char data[4096]; ssize_t n = read(fds[i].fd, data, sizeof(data) - 1);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) break;
    if (n <= 0) fail("capture read");
    data[n] = 0; printf("%s %s", i ? "KMSG" : "USBMON", data);
    if (!i || attempted) continue;
    char *enumeration = strstr(data, "usb 1-1: new full-speed USB device number ");
    if (enumeration) {
     unsigned int number = 0; int consumed = 0;
     if (sscanf(enumeration, "usb 1-1: new full-speed USB device number %u using xhci_hcd%n",
                &number, &consumed) != 1 || !consumed || !number || number > 127 || observed_devnum)
      fail("unexpected boot enumeration");
     observed_devnum = number;
    }
    if (strstr(data, "usb 1-1: USB disconnect")) fail("observed boot epoch disconnected");
    if (strstr(data, identity_line)) identity = 1;
    if (!strstr(data, "ws73 1-1:1.0: boot version observation: 12 bytes 424f4f545645523030310000")) continue;
    if (!identity || !observed_devnum) fail("initial selected identity/epoch missing");
    read_text("/sys/bus/usb/devices/1-1/serial", value, sizeof(value));
    if (strcmp(serial, value)) fail("guest serial mismatch");
    read_text("/sys/bus/usb/devices/1-1/idVendor", value, sizeof(value));
    if (strcmp(value, "ffff")) fail("guest vendor mismatch");
    read_text("/sys/bus/usb/devices/1-1/idProduct", value, sizeof(value));
    if (strcmp(value, "3733")) fail("guest product mismatch");
    read_text("/sys/bus/usb/devices/1-1/devnum", value, sizeof(value));
    char *end; unsigned long current_devnum = strtoul(value, &end, 10);
    if (*end || current_devnum != observed_devnum) fail("guest enumeration changed");
    attempted = 1;
    require_observer();
    snprintf(parameters, sizeof(parameters), "serial=\"%s\" devnum=%u package=%u", serial, observed_devnum, package);
    load_module("/trial.ko", parameters, 0);
    require_observer();
    printf("WS73_RAM_TRIAL_REBIND board=%s package=%u reference_xo=8,60\n", board, package);
    bind_command("/sys/bus/usb/drivers/ws73/unbind");
    bind_command("/sys/bus/usb/drivers/ws73-ram-trial/bind");
   }
  }
 }
}
