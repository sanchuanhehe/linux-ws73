// SPDX-License-Identifier: GPL-2.0-only
/* Synthetic FunctionFS device, run ONLY as PID 1 in the hardware-free VM. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <linux/usb/functionfs.h>

#define G "/sys/kernel/config/usb_gadget/ws73/"
static int disconnected;

static void stop(int failed)
{
 printf("WS73_VIRTUAL_USB_%s\n", failed ? "FAIL" : "PASS");
 reboot(RB_POWER_OFF);
 for (;;) pause();
}
static void die(const char *what)
{
 fprintf(stderr, "WS73_TEST_ERROR %s: %s\n", what, strerror(errno));
 stop(1);
}
static void directory(const char *path)
{
 if (mkdir(path, 0755) && errno != EEXIST) die(path);
}
static void setting(const char *path, const char *value)
{
 int fd = open(path, O_WRONLY);
 if (fd < 0) die(path);
 if (write(fd, value, strlen(value)) != (ssize_t)strlen(value)) die(path);
 close(fd);
}
static void le16(unsigned char *p, unsigned int v) { p[0] = v; p[1] = v >> 8; }
static void le32(unsigned char *p, uint32_t v) { le16(p, v); le16(p + 2, v >> 16); }
static void descriptors(int fd)
{
 unsigned char d[108] = {0}, s[23] = {0};
 le32(d, FUNCTIONFS_DESCRIPTORS_MAGIC_V2); le32(d + 4, sizeof(d));
 le32(d + 8, FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC);
 le32(d + 12, 6); le32(d + 16, 6);
 for (unsigned int speed = 0; speed < 2; speed++) {
  unsigned char *p = d + 20 + speed * 44;
  unsigned char iface[] = {9, USB_DT_INTERFACE, 0, 0, 5, 0xe0, 2, 2, 1};
  const unsigned char addresses[] = {0x81, 0x02, 0x83, 0x04, 0x85};
  memcpy(p, iface, sizeof(iface)); p += sizeof(iface);
  for (unsigned int i = 0; i < 5; i++, p += 7) {
   p[0] = 7; p[1] = USB_DT_ENDPOINT; p[2] = addresses[i];
   p[3] = i == 2 ? USB_ENDPOINT_XFER_INT : USB_ENDPOINT_XFER_BULK;
   le16(p + 4, i == 2 ? 8 : speed ? 512 : 64);
   p[6] = i == 2 ? speed ? 4 : 1 : 0;
  }
 }
 if (write(fd, d, sizeof(d)) != sizeof(d)) die("FunctionFS descriptors");
 le32(s, FUNCTIONFS_STRINGS_MAGIC); le32(s + 4, sizeof(s));
 le32(s + 8, 1); le32(s + 12, 1); le16(s + 16, 0x409);
 memcpy(s + 18, "ws73", 5);
 if (write(fd, s, sizeof(s)) != sizeof(s)) die("FunctionFS strings");
}
static void enabled(int fd)
{
 for (;;) {
  struct usb_functionfs_event event;
  ssize_t n = read(fd, &event, sizeof(event));
  if (n != sizeof(event)) die("FunctionFS event");
  if (event.type == FUNCTIONFS_ENABLE) return;
  if (event.type == FUNCTIONFS_SETUP) die("unexpected control SETUP");
 }
}
static void wait_kernel(int fd, const char *wanted)
{
 struct timespec start, now;
 clock_gettime(CLOCK_MONOTONIC, &start);
 for (;;) {
  char line[4096];
  ssize_t n = read(fd, line, sizeof(line) - 1);
  if (n > 0) {
   line[n] = 0;
   int removing = strstr(line, "USB disconnect") != NULL;
   if (removing) { disconnected = 1; printf("WS73_TEST_KMSG %s", line); }
   int observation = strstr(line, "function notification observation") ||
    strstr(line, "BSP notification observation") || strstr(line, "interrupt observation failed:");
   if (observation)
    printf("WS73_TEST_KMSG %s", line);
   if (strstr(line, wanted)) {
    if (!strcmp(wanted, "runtime chip observation failed: ") &&
        !strstr(line, "runtime chip observation failed: -19\n") &&
        !strstr(line, "runtime chip observation failed: -108\n") &&
        !strstr(line, "runtime chip observation failed: -71\n")) die("unexpected removal status");
    if (!removing && !observation)
     printf("WS73_TEST_KMSG %s", line);
    return;
   }
  } else if (n < 0 && errno != EAGAIN && errno != EINTR && errno != EPIPE) {
   die("kernel log");
  }
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (now.tv_sec - start.tv_sec >= 5) die(wanted);
  if (n <= 0) {
   struct pollfd p = { .fd = fd, .events = POLLIN };
   poll(&p, 1, 20);
  }
 }
}
static void transaction(int out, unsigned int step)
{
 unsigned char got[512], expected[128] = {0};
 ssize_t n = read(out, got, sizeof(got));
 unsigned int length = step < 2 ? 36 : 128;
 if (n != length) die("OUT actual length");
 le32(expected, step < 2 ? 3 : 0);
 le32(expected + 4, step < 2 ? 0 : 128);
 le32(expected + 8, step);
 if (step < 2) { le32(expected + 12, 1 - step); le32(expected + 16, step ? 0 : 9); }
 else {
  expected[64] = 0x19; expected[65] = 9; expected[66] = 32;
  expected[68] = step == 2 ? 16 : 13; expected[70] = 1;
  expected[72] = step == 2 ? 0 : 1;
 }
 if (memcmp(got, expected, length)) die("OUT wire bytes");
 printf("WS73_TEST_OUT step=%u actual=%zd verified\n", step, n);
}
static void notifications(int in, unsigned int trial)
{
 unsigned char wire[156] = {0};
 unsigned int count = trial == 8 ? 4 : trial == 9 ? 1 : 2;
 unsigned int length = 92 + count * 16;
 le32(wire, 2); le32(wire + 4, length); le32(wire + 8, 10);
 for (unsigned int i = 0; i < count; i++) {
  unsigned char *p = wire + 92 + i * 16;
  le16(wire + 12 + i * 2, 16);
  p[0] = 0x50; p[1] = 10; le16(p + 2, 8);
  le16(p + 4, i < 2 ? 2 : 4); le16(p + 6, 4);
  le32(p + 8, i < 2 ? i + 1 : i - 2);
 }
 if (trial == 8) {
  /* Unsupported SLE action after a valid status: publish neither. */
  le16(wire + 92 + 16 + 4, 4); le32(wire + 92 + 16 + 8, 2);
  if (write(in, wire, length) != length) die("unsupported notification IN");
  printf("WS73_TEST_IN kind=unsupported actual=%u synthetic\n", length);
  le16(wire + 92 + 16 + 4, 2); le32(wire + 92 + 16 + 8, 2);
 } else if (trial == 9) le16(wire + 92 + 6, 3);
 else le32(wire + 92 + 16 + 8, 0xffffffff);
 if (write(in, wire, length) != length) die("notification IN");
 printf("WS73_TEST_IN kind=notification actual=%u synthetic\n", length);
 if (trial == 8) {
  if (write(in, wire, length) != length) die("duplicate notification IN");
  printf("WS73_TEST_IN kind=duplicate actual=%u synthetic\n", length);
 }
}
static void interrupt_event(int in, int log, unsigned int trial)
{
 unsigned char wire[8] = {0};
 unsigned int length = trial == 12 ? 7 : 8;
 uint32_t bits = trial == 11 ? 0x80000819 : trial == 14 ? 0x40001 :
                 trial == 15 ? 0x21 : 0x41;
 if (trial == 11) {
  le32(wire + 4, 0xffffffff);
  if (write(in, wire, 8) != 8) die("empty interrupt IN");
  puts("WS73_TEST_INT kind=empty actual=8 bits=00000000 synthetic");
 }
 le32(wire, bits); le32(wire + 4, 0xabcdef12);
 if (write(in, wire, length) != length) die("interrupt IN");
 printf("WS73_TEST_INT kind=event actual=%u bits=%08x synthetic\n", length, bits);
 if (trial == 11) {
  wait_kernel(log, "BSP notification observation; pool 0xabcdef12 is not a TX credit");
  if (write(in, wire, 8) != 8) die("duplicate interrupt IN");
  puts("WS73_TEST_INT kind=duplicate actual=8 bits=80000819 synthetic");
 }
}
int main(void)
{
 if (getpid() != 1) return 2;
 int console = open("/dev/console", O_RDWR);
 if (console < 0) return 2;
 dup2(console, 0); dup2(console, 1); dup2(console, 2);
 setvbuf(stdout, NULL, _IONBF, 0);
 if (mount("proc", "/proc", "proc", 0, NULL) ||
     mount("sysfs", "/sys", "sysfs", 0, NULL) ||
     mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) ||
     mount("configfs", "/sys/kernel/config", "configfs", 0, NULL)) die("mount");
 int report = open("/dev/ttyS1", O_RDWR);
 struct termios tty;
 if (report < 0 || tcgetattr(report, &tty)) die("report serial");
 cfmakeraw(&tty); cfsetospeed(&tty, B115200); cfsetispeed(&tty, B115200);
 if (tcsetattr(report, TCSANOW, &tty)) die("report termios");
 dup2(report, 1); dup2(report, 2);
 int log = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
 if (log < 0) die("open kmsg");
 char udc[128] = {0};
 DIR *dir = opendir("/sys/class/udc"); struct dirent *entry;
 if (!dir) die("UDC inventory");
 while ((entry = readdir(dir))) {
  if (entry->d_name[0] == '.') continue;
  if (strncmp(entry->d_name, "dummy_udc.", 10) || udc[0]) die("non-synthetic or multiple UDCs");
  if (strlen(entry->d_name) >= sizeof(udc)) die("UDC name");
  strcpy(udc, entry->d_name);
 }
 closedir(dir);
 if (!udc[0]) die("missing dummy UDC");
 directory(G); setting(G "idVendor", "0xffff"); setting(G "idProduct", "0x3733");
 setting(G "bcdUSB", "0x0200");
 directory(G "strings/0x409"); setting(G "strings/0x409/serialnumber", "WS73-VM-SYNTHETIC");
 directory(G "configs/c.1"); directory(G "functions/ffs.ws73"); directory("/dev/ffs");
 if (mount("ws73", "/dev/ffs", "functionfs", 0, NULL)) die("mount FunctionFS");
 int control = open("/dev/ffs/ep0", O_RDWR);
 if (control < 0) die("ep0");
 descriptors(control);
 int eps[5];
 for (unsigned int i = 0; i < 5; i++) {
  char path[32]; snprintf(path, sizeof(path), "/dev/ffs/ep%u", i + 1);
  eps[i] = open(path, O_RDWR);
  if (eps[i] < 0) die(path);
 }
 if (symlink(G "functions/ffs.ws73", G "configs/c.1/ffs.ws73")) die("function link");
 for (unsigned int trial = 0; trial < 17; trial++) {
  unsigned char reply[124] = {0};
  const char *expected[] = {
   "runtime chip observation: package 2 bus 0; firmware compatibility unverified",
   "runtime chip observation: package 0 bus 0; firmware compatibility unverified",
   "runtime chip observation failed: -121",
   "runtime chip observation failed: -90",
   "runtime chip observation failed: ",
   "runtime version observation failed: -121",
   "runtime version observation failed: -90",
   "runtime chip observation: package 2 bus 0; firmware compatibility unverified",
   "runtime chip observation: package 2 bus 0; firmware compatibility unverified",
   "runtime version observation failed: -90",
   "runtime version observation failed: -71",
   "runtime chip observation: package 2 bus 0; firmware compatibility unverified",
   "runtime version observation failed: -90",
   "runtime version observation failed: -5",
   "runtime version observation failed: -5",
   "runtime version observation failed: -105",
   "runtime chip observation: package 2 bus 0; firmware compatibility unverified",
  };
  printf("WS73_TEST_CASE %u BEGIN\n", trial); disconnected = 0;
  setting(G "UDC", udc); enabled(control);
  for (unsigned int step = 0; step < 3; step++) transaction(eps[1], step);
  if (trial == 4) { puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); }
  else {
   le32(reply, 2); le32(reply + 4, sizeof(reply)); le32(reply + 8, 9);
   le16(reply + 12, 32); reply[92] = 0x19; reply[93] = 9;
   le16(reply + 94, 32); le16(reply + 96, 10); le16(reply + 98, 10);
   le32(reply + 100, trial == 2 ? 1 : 0); le32(reply + 104, 2);
   reply[108] = trial == 1 ? 0 : 2;
   size_t length = trial == 3 ? sizeof(reply) - 1 : sizeof(reply);
   if (write(eps[0], reply, length) != (ssize_t)length) die("IN actual length");
   printf("WS73_TEST_IN kind=chip actual=%zu synthetic\n", length);
  }
  if (trial == 0 || trial == 1 || trial >= 5) {
   transaction(eps[1], 3);
   if (trial == 7) {
    if (write(eps[0], reply, sizeof(reply)) != sizeof(reply)) die("stale chip IN");
    puts("WS73_TEST_IN kind=stale actual=124 synthetic");
   }
   if (trial >= 8 && trial <= 10) notifications(eps[0], trial);
   if (trial >= 11 && trial <= 15) interrupt_event(eps[2], log, trial);
   if (trial < 9 || trial == 11 || trial == 16) {
   memset(reply + 92, 0, 32);
   reply[92] = 0x19; reply[93] = 9; le16(reply + 94, 32);
   le16(reply + 96, 7); le16(reply + 98, 16);
   le32(reply + 100, trial == 5 ? 1 : 0);
   le32(reply + 104, trial == 6 ? 7 : 8);
   le32(reply + 108, 0x010a6e7f); le32(reply + 112, 0x12345678);
   if (write(eps[0], reply, sizeof(reply)) != sizeof(reply)) die("version IN");
   puts("WS73_TEST_IN kind=version actual=124 synthetic");
   }
  }
  wait_kernel(log, expected[trial]);
  if (trial == 0 || trial == 1 || trial == 7 || trial == 8 || trial == 11 || trial == 16)
   wait_kernel(log, "runtime version observation: firmware 0x010a6e7f ROM 0x12345678");
  if (trial == 16) {
   interrupt_event(eps[2], log, trial);
   wait_kernel(log, "interrupt observation failed: -5");
  }
  if (trial != 4) { puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); }
  if (!disconnected) wait_kernel(log, "USB disconnect");
  wait_kernel(log, "device callbacks drained");
  printf("WS73_TEST_CASE %u PASS\n", trial);
 }
 close(log);
 for (unsigned int i = 0; i < 5; i++) close(eps[i]);
 close(control);
 stop(0);
}
