// SPDX-License-Identifier: GPL-2.0-only
/* Tagged VM PID 1 only: observe, explicitly rebind one U, capture and stop. */
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
 char serial[128], value[128], parameters[160];
 read_text("/sys/class/dmi/id/sys_vendor", value, sizeof(value));
 if (strcmp(value, "WS73-VM")) fail("VM vendor guard");
 read_text("/sys/class/dmi/id/product_name", value, sizeof(value));
 if (strcmp(value, "WS73-U-trial")) fail("VM product guard");
 read_text("/trial-serial", serial, sizeof(serial));
 if (strpbrk(serial, "\"\\ \t")) fail("serial parameter syntax");
 snprintf(parameters, sizeof(parameters), "serial=\"%s\"", serial);
 int module = open("/trial.ko", O_RDONLY);
 if (module < 0 || syscall(SYS_finit_module, module, parameters, 0)) fail("VM trial module");
 close(module);
 int monitor = open("/sys/kernel/debug/usb/usbmon/0u", O_RDONLY | O_NONBLOCK);
 int kmsg = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
 if (monitor < 0 || kmsg < 0) fail("capture");
 puts("WS73_RAM_TRIAL_READY_NO_DEVICE");
 struct timespec start, now;
 clock_gettime(CLOCK_MONOTONIC, &start);
 int identity = 0, attempted = 0;
 for (;;) {
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (now.tv_sec - start.tv_sec >= 90) fail("absolute VM deadline");
  struct pollfd fds[] = {{monitor, POLLIN, 0}, {kmsg, POLLIN, 0}};
  if (poll(fds, 2, 100) < 0 && errno != EINTR) fail("capture poll");
  for (unsigned int i = 0; i < 2; i++) {
   for (;;) {
    char data[4096]; ssize_t n = read(fds[i].fd, data, sizeof(data) - 1);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) break;
    if (n <= 0) fail("capture read");
    data[n] = 0; printf("%s %s", i ? "KMSG" : "USBMON", data);
    if (!i || attempted) continue;
    if (strstr(data, "boot chip observation 0x00000000: package 0 bus 0;")) identity = 1;
    if (!strstr(data, "boot version observation: 12 bytes 424f4f545645523030310000")) continue;
    if (!identity) fail("initial U identity missing");
    read_text("/sys/bus/usb/devices/1-1/serial", value, sizeof(value));
    if (strcmp(serial, value)) fail("guest serial mismatch");
    read_text("/sys/bus/usb/devices/1-1/idVendor", value, sizeof(value));
    if (strcmp(value, "ffff")) fail("guest vendor mismatch");
    read_text("/sys/bus/usb/devices/1-1/idProduct", value, sizeof(value));
    if (strcmp(value, "3733")) fail("guest product mismatch");
    attempted = 1;
    puts("WS73_RAM_TRIAL_REBIND_ONE_U reference_xo=8,60");
    bind_command("/sys/bus/usb/drivers/ws73/unbind");
    bind_command("/sys/bus/usb/drivers/ws73-ram-trial/bind");
   }
  }
 }
}
