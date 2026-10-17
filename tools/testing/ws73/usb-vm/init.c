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
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <linux/usb/functionfs.h>

#define G "/sys/kernel/config/usb_gadget/ws73/"
#ifndef WS73_TEST_BOOT
#define WS73_TEST_BOOT 0
#endif
#ifndef WS73_TEST_DOWNLOAD
#define WS73_TEST_DOWNLOAD 0
#endif
#ifndef WS73_TEST_LOADER
#define WS73_TEST_LOADER 0
#endif
static int disconnected;
static volatile sig_atomic_t alarm_seen;

static void quiet_alarm(int signal)
{
 (void)signal;
 alarm_seen = 1;
}

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
 unsigned int endpoints = (WS73_TEST_BOOT || WS73_TEST_DOWNLOAD || WS73_TEST_LOADER) ? 2 : 5;
 unsigned int stride = 9 + endpoints * 7, length = 20 + stride * 2;
 le32(d, FUNCTIONFS_DESCRIPTORS_MAGIC_V2); le32(d + 4, length);
 le32(d + 8, FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC);
 le32(d + 12, endpoints + 1); le32(d + 16, endpoints + 1);
 for (unsigned int speed = 0; speed < 2; speed++) {
  unsigned char *p = d + 20 + speed * stride;
  unsigned char iface[] = {9, USB_DT_INTERFACE, 0, 0, endpoints, 0xe0, 2, 2, 1};
  const unsigned char addresses[] = {0x81, 0x02, 0x83, 0x04, 0x85};
  memcpy(p, iface, sizeof(iface)); p += sizeof(iface);
  for (unsigned int i = 0; i < endpoints; i++, p += 7) {
   p[0] = 7; p[1] = USB_DT_ENDPOINT; p[2] = addresses[i];
   p[3] = i == 2 ? USB_ENDPOINT_XFER_INT : USB_ENDPOINT_XFER_BULK;
   le16(p + 4, i == 2 ? 8 : speed ? 512 : 64);
   p[6] = i == 2 ? speed ? 4 : 1 : 0;
  }
 }
 if (write(fd, d, length) != length) die("FunctionFS descriptors");
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
static void boot_trials(int control, int *eps, int log, const char *udc)
{
 struct sigaction action = { .sa_handler = quiet_alarm };
 if (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL)) die("quiet signal");
 static const uint32_t words[] = {0, 0x0a, 0x0c, 4, 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0x0a};
 static const unsigned int lengths[] = {4, 4, 4, 4, 4, 0, 1, 3, 5, 512, 0, 0, 4, 4};
 static const char *expected[] = {
  "boot chip observation 0x00000000: package 0 bus 0; cold-read qualification pending",
  "boot chip observation 0x0000000a: package 2 bus 0; cold-read qualification pending",
  "boot chip observation 0x0000000c: package 0 bus 0; cold-read qualification pending",
  "boot chip observation 0x00000004: package 1 bus 1; cold-read qualification pending",
  "boot chip observation 0x00000028: package 2 bus 2; cold-read qualification pending",
 };
 for (unsigned int trial = 0; trial < 14; trial++) {
  unsigned char got[512], reply[512] = {0};
  static const char command[] = "READM 0x40019000 4 ";
  const char *message;
  printf("WS73_BOOT_CASE %u BEGIN\n", trial); disconnected = 0;
  setting(G "UDC", udc); enabled(control);
  ssize_t n = read(eps[1], got, sizeof(got));
  if (n != sizeof(command) - 1 || memcmp(got, command, sizeof(command) - 1))
   die("boot READM bytes/length");
  printf("WS73_BOOT_OUT actual=%zd READM_40019000_4 verified\n", n);
  if (trial == 11) { puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); }
  else if (trial != 10) {
   le32(reply, words[trial]);
   if (write(eps[0], reply, lengths[trial]) != lengths[trial]) die("boot IN length");
   printf("WS73_BOOT_IN actual=%u word=%08x synthetic\n", lengths[trial], words[trial]);
  } else puts("WS73_BOOT_WITHHOLD_REPLY");
  message = trial < 5 ? expected[trial] : trial < 10 ? "boot chip observation failed: -90" :
            trial == 10 ? "boot chip observation failed: -110" :
            trial == 11 ? "boot chip observation failed: " : expected[trial - 12];
  wait_kernel(log, message);
  if (trial < 5 || trial > 11) {
   n = read(eps[1], got, sizeof(got));
   if (n != 8 || memcmp(got, "VERSION ", 8)) die("boot VERSION bytes/length");
   puts("WS73_BOOT_VERSION_OUT actual=8 verified");
   unsigned int version_len = trial == 0 || trial == 12 ? 64 : 3;
   if (version_len == 64) memset(reply, 0x56, version_len);
   else memcpy(reply, "V1", version_len);
   if (write(eps[0], reply, version_len) != version_len) die("boot VERSION reply");
   printf("WS73_BOOT_VERSION_IN actual=%u synthetic\n", version_len);
   wait_kernel(log, version_len == 64 ? "boot version observation: 64 bytes " :
               "boot version observation: 3 bytes 563100");
  }
  if (trial != 11) {
   /* FunctionFS O_NONBLOCK only governs locks/enable, not queued IO.
    * Interrupt this read after a 100-ms quiet window; FunctionFS dequeues
    * and joins its request before returning EINTR. Repeat the timer so a
    * signal delivered before read admission cannot leave it stuck.
    */
   struct itimerval timer = { .it_interval.tv_usec = 100000, .it_value.tv_usec = 100000 };
   alarm_seen = 0;
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("quiet timer");
   n = read(eps[1], got, sizeof(got));
   int saved_errno = errno;
   memset(&timer, 0, sizeof(timer));
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("quiet timer stop");
   if (n != -1 || saved_errno != EINTR || !alarm_seen) die("unexpected extra boot OUT");
   puts("WS73_BOOT_NO_EXTRA_OUT window_ms=100");
   puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n");
  }
  if (!disconnected) wait_kernel(log, "USB disconnect");
  wait_kernel(log, "device callbacks drained");
  printf("WS73_BOOT_CASE %u PASS\n", trial);
 }
}
static void download_trials(int control, int *eps, int log, const char *udc)
{
 struct sigaction action = { .sa_handler = quiet_alarm };
 static const char *commands[] = {
  "FILES 1 0x400000 0x8000 0x0 ", "FILES 1 0x408000 0x1 0x2 ",
  "FILES 1 0x430000 0x20 0x3 ", "FILES 1 0x440000 0x20 0x3 ",
 };
 static const unsigned int sizes[] = {32768, 1, 32, 32};
 if (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL)) die("download signal");
 for (unsigned int trial = 0; trial < 7; trial++) {
  unsigned char got[32768];
  const char *result = trial == 0 || trial == 6 ? "synthetic download result=0 stage=6 image=2 ack=32" :
   trial == 1 ? "synthetic download result=-71 stage=2 image=0 ack=0" :
   trial == 2 ? "synthetic download result=-71 stage=4 image=0 ack=0" :
   trial == 3 ? "synthetic download result=-110 stage=2 image=0 ack=0" : "synthetic download result=";
  printf("WS73_DOWNLOAD_CASE %u BEGIN\n", trial); disconnected = 0;
  setting(G "UDC", udc); enabled(control);
  struct usb_endpoint_descriptor endpoint;
  if (ioctl(eps[1], FUNCTIONFS_ENDPOINT_DESC, &endpoint)) die("download endpoint descriptor");
  const unsigned char *raw = (const unsigned char *)&endpoint;
  unsigned int packet_size = (raw[4] | raw[5] << 8) & 0x7ff;
  if (packet_size != 64 && packet_size != 512) die("download packet size");
  printf("WS73_DOWNLOAD_PACKET_SIZE %u\n", packet_size);
  for (unsigned int chunk = 0; chunk < 4; chunk++) {
   ssize_t n = read(eps[1], got, sizeof(got));
   if (n != (ssize_t)strlen(commands[chunk]) || memcmp(got, commands[chunk], n)) die("FILES wire");
   printf("WS73_DOWNLOAD_COMMAND chunk=%u actual=%zd verified\n", chunk, n);
   if (trial == 3) { puts("WS73_DOWNLOAD_WITHHOLD_READY"); break; }
   if (trial == 4) { puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); break; }
   struct timespec ready, first;
   clock_gettime(CLOCK_MONOTONIC, &ready);
   const char *reply = trial == 1 ? "READX" : "READY";
   if (write(eps[0], reply, 5) != 5) die("READY IN");
   printf("WS73_DOWNLOAD_REPLY %s actual=5\n", reply);
   if (trial == 1) break;
   if (trial == 5) {
    wait_kernel(log, "synthetic payload admission");
    puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); break;
   }
   unsigned int first_size = sizes[chunk] > packet_size ? packet_size : sizes[chunk];
   n = read(eps[1], got, first_size);
   clock_gettime(CLOCK_MONOTONIC, &first);
   if (n != first_size) die("first payload length");
   long elapsed = (first.tv_sec - ready.tv_sec) * 1000000L + (first.tv_nsec - ready.tv_nsec) / 1000;
   if (elapsed < 5000) die("READY wait below 5ms");
   if (sizes[chunk] > first_size) {
    n = read(eps[1], got + first_size, sizes[chunk] - first_size);
    if (n != sizes[chunk] - first_size) die("remaining payload length");
   }
   for (unsigned int j = 0; j < sizes[chunk]; j++) {
    unsigned int index = j + (chunk == 1 ? 32768 : 0);
    unsigned int image = chunk < 2 ? 0 : chunk - 1;
    if (got[j] != (unsigned char)(index * 37 + 11 + image * 19)) die("payload bytes");
   }
   printf("WS73_DOWNLOAD_PAYLOAD chunk=%u actual=%u delay_us=%ld verified\n", chunk, sizes[chunk], elapsed);
   reply = trial == 2 ? "FILES NO" : "FILES OK";
   if (write(eps[0], reply, 8) != 8) die("FILES result IN");
   printf("WS73_DOWNLOAD_REPLY %s actual=8\n", reply);
   if (trial == 2) break;
  }
  if (trial == 0 || trial == 6) {
   ssize_t n = read(eps[1], got, sizeof(got));
   if (n != 5 || memcmp(got, "QUIT ", 5)) die("QUIT wire");
   puts("WS73_DOWNLOAD_QUIT actual=5 verified");
  }
  wait_kernel(log, result);
  if (trial != 4 && trial != 5) {
   struct itimerval timer = { .it_interval.tv_usec = 100000, .it_value.tv_usec = 100000 };
   alarm_seen = 0;
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("download quiet timer");
   ssize_t n = read(eps[1], got, sizeof(got));
   int saved_errno = errno;
   memset(&timer, 0, sizeof(timer));
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("download timer stop");
   if (n != -1 || saved_errno != EINTR || !alarm_seen) die("extra download OUT");
   puts("WS73_DOWNLOAD_NO_EXTRA_OUT window_ms=100");
   puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n");
  }
  if (!disconnected) wait_kernel(log, "USB disconnect");
  wait_kernel(log, "synthetic download callbacks drained");
  printf("WS73_DOWNLOAD_CASE %u PASS\n", trial);
 }
}
#define FWROOT "/lib/firmware/hisilicon/ws73/1.10.110/"
struct loader_blob { unsigned char *data; size_t size; };
static struct loader_blob loader_file(const char *path, size_t expected)
{
 struct loader_blob blob = { .size = expected };
 struct stat status;
 int fd = open(path, O_RDONLY);
 if (fd < 0 || fstat(fd, &status) || status.st_size != (off_t)expected) die("loader input size");
 blob.data = malloc(expected);
 if (!blob.data) die("loader input allocation");
 size_t done = 0;
 while (done < expected) {
  ssize_t n = read(fd, blob.data + done, expected - done);
  if (n <= 0) die("loader input read");
  done += n;
 }
 close(fd);
 return blob;
}
static void loader_install(const char *path, struct loader_blob blob)
{
 int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
 if (fd < 0) die("loader input install");
 size_t done = 0;
 while (done < blob.size) {
  ssize_t n = write(fd, blob.data + done, blob.size - done);
  if (n <= 0) die("loader input write");
  done += n;
 }
 if (close(fd)) die("loader input close");
}
static void loader_trials(int control, int *eps, int log, const char *udc)
{
 static const char *paths[2][3] = {
  { FWROOT "us/ws73.bin", FWROOT "us/wifi_cali.bin", FWROOT "us/btc_cali.bin" },
  { FWROOT "e/ws73.bin", FWROOT "e/wifi_cali.bin", FWROOT "e/btc_cali.bin" },
 };
 static const unsigned int sizes[2][3] = {{143956,21044,7772},{137644,21060,34120}};
 static const unsigned int address[] = {0x400000,0x430000,0x440000};
 static const char *writes[] = {
  "WRITEM 4 0x40017750 51 ", "WRITEM 4 0x40017790 0 ", "WRITEM 4 0x40017794 0 ",
  "WRITEM 4 0x4001775c 4390912 ", "WRITEM 4 0x40017760 4423680 ",
  "WRITEM 4 0x40017750 63 ", "WRITEM 4 0x40017750 47 ", "WRITEM 4 0x40017798 0 ",
  "WRITEM 4 0x40017764 4456448 ", "WRITEM 4 0x40017750 63 ", "WRITEM 4 0x40019408 513 ",
 };
 static const uint32_t words[] = {0,10,0,0,0,4,40,0,0,0};
 struct loader_blob blobs[2][3], mutant = loader_file("/mutant.bin", 143956);
 struct sigaction action = { .sa_handler = quiet_alarm };
 if (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL)) die("loader signal");
 for (unsigned int m = 0; m < 2; m++)
  for (unsigned int i = 0; i < 3; i++) blobs[m][i] = loader_file(paths[m][i], sizes[m][i]);
 for (unsigned int trial = 0; trial < 10; trial++) {
  unsigned char got[32768], reply[4];
  unsigned int model = trial == 1;
  printf("WS73_LOADER_CASE %u BEGIN\n", trial); disconnected = 0;
  if (trial == 2 && unlink(paths[0][2])) die("remove BTC input");
  if (trial == 3) loader_install(paths[0][0], mutant);
  if (trial == 4) loader_install(paths[0][1], blobs[1][1]);
  setting(G "UDC", udc); enabled(control);
  ssize_t n = read(eps[1], got, sizeof(got));
  if (n != 19 || memcmp(got, "READM 0x40019000 4 ", 19)) die("loader identity before writes");
  puts("WS73_LOADER_READM actual=19 verified");
  if (trial == 7) { puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n"); }
  else {
   le32(reply, words[trial]);
   if (write(eps[0], reply, 4) != 4) die("loader identity reply");
   printf("WS73_LOADER_ID actual=4 word=%08x synthetic\n", words[trial]);
  }
  if (trial == 0 || trial == 1 || trial == 8 || trial == 9) {
   for (unsigned int step = 0; step < 11; step++) {
    n = read(eps[1], got, sizeof(got));
    if (n != (ssize_t)strlen(writes[step]) || memcmp(got, writes[step], n)) die("board write bytes");
    printf("WS73_LOADER_WRITEM step=%u actual=%zd verified\n", step, n);
    const char *ack = trial == 8 ? "WRITEM NO" : "WRITEM OK";
    if (write(eps[0], ack, 9) != 9) die("board reply");
    printf("WS73_LOADER_REPLY %s actual=9\n", ack);
    if (trial == 8) break;
   }
  }
  if (trial == 0 || trial == 1 || trial == 9) {
   for (unsigned int image = 0; image < 3; image++) {
    size_t payload = sizes[model][image] - 64, offset = 0;
    while (offset < payload) {
     char command[80];
     size_t amount = payload - offset > 32768 ? 32768 : payload - offset;
     unsigned int flag = !offset ? (amount == payload ? 3 : 0) : amount == payload - offset ? 2 : 1;
     int length = snprintf(command, sizeof(command), "FILES 1 0x%x 0x%zx 0x%x ", address[image] + (unsigned int)offset, amount, flag);
     n = read(eps[1], got, sizeof(got));
     if (n != length || memcmp(got, command, length)) die("loader FILES bytes");
     printf("WS73_LOADER_FILES image=%u offset=%zu size=%zu flag=%u verified\n", image, offset, amount, flag);
     if (write(eps[0], "READY", 5) != 5) die("loader READY");
     n = read(eps[1], got, amount);
     if (n != (ssize_t)amount || memcmp(got, blobs[model][image].data + 64 + offset, amount)) die("original payload bytes");
     printf("WS73_LOADER_PAYLOAD image=%u offset=%zu actual=%zd original\n", image, offset, n);
     if (write(eps[0], "FILES OK", 8) != 8) die("loader FILES OK");
     puts("WS73_LOADER_FILES_OK actual=8");
     offset += amount;
    }
   }
   n = read(eps[1], got, sizeof(got));
   if (n != 5 || memcmp(got, "QUIT ", 5)) die("loader QUIT");
   puts("WS73_LOADER_QUIT actual=5 verified");
  }
  wait_kernel(log, "synthetic loader result=");
  if (trial != 7) {
   struct itimerval timer = { .it_interval.tv_usec = 100000, .it_value.tv_usec = 100000 };
   alarm_seen = 0;
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("loader quiet timer");
   n = read(eps[1], got, sizeof(got));
   int saved_errno = errno;
   memset(&timer, 0, sizeof(timer));
   if (setitimer(ITIMER_REAL, &timer, NULL)) die("loader timer stop");
   if (n != -1 || saved_errno != EINTR || !alarm_seen) die("extra loader OUT");
   puts("WS73_LOADER_NO_EXTRA_OUT window_ms=100");
   puts("WS73_TEST_REMOVE"); setting(G "UDC", "\n");
  }
  if (!disconnected) wait_kernel(log, "USB disconnect");
  wait_kernel(log, "synthetic download callbacks drained");
  for (unsigned int i = 0; i < 3; i++) loader_install(paths[0][i], blobs[0][i]);
  printf("WS73_LOADER_CASE %u PASS\n", trial);
 }
 for (unsigned int m = 0; m < 2; m++)
  for (unsigned int i = 0; i < 3; i++) free(blobs[m][i].data);
 free(mutant.data);
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
 directory(G); setting(G "idVendor", "0xffff"); setting(G "idProduct", WS73_TEST_LOADER ? "0x3735" : WS73_TEST_DOWNLOAD ? "0x3734" : "0x3733");
 setting(G "bcdUSB", "0x0200");
 directory(G "strings/0x409"); setting(G "strings/0x409/serialnumber", "WS73-VM-SYNTHETIC");
 directory(G "configs/c.1"); directory(G "functions/ffs.ws73"); directory("/dev/ffs");
 if (mount("ws73", "/dev/ffs", "functionfs", 0, NULL)) die("mount FunctionFS");
 int control = open("/dev/ffs/ep0", O_RDWR);
 if (control < 0) die("ep0");
 descriptors(control);
 int eps[5];
 unsigned int endpoints = (WS73_TEST_BOOT || WS73_TEST_DOWNLOAD || WS73_TEST_LOADER) ? 2 : 5;
 for (unsigned int i = 0; i < endpoints; i++) {
  char path[32]; snprintf(path, sizeof(path), "/dev/ffs/ep%u", i + 1);
  eps[i] = open(path, O_RDWR);
  if (eps[i] < 0) die(path);
 }
 if (symlink(G "functions/ffs.ws73", G "configs/c.1/ffs.ws73")) die("function link");
 if (WS73_TEST_LOADER) loader_trials(control, eps, log, udc);
 else if (WS73_TEST_DOWNLOAD) download_trials(control, eps, log, udc);
 else if (WS73_TEST_BOOT) boot_trials(control, eps, log, udc);
 else for (unsigned int trial = 0; trial < 17; trial++) {
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
 for (unsigned int i = 0; i < endpoints; i++) close(eps[i]);
 close(control);
 stop(0);
}
