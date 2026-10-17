/* SPDX-License-Identifier: GPL-2.0-only */
/* Bounded binary usbmon capture in the isolated test guest, never the host. */
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

struct ws73_mon_header {
 uint64_t id;
 uint8_t type, transfer, endpoint, device;
 uint16_t bus;
 uint8_t setup_flag, data_flag;
 int64_t seconds;
 int32_t microseconds, status;
 uint32_t length, captured;
 uint8_t setup[8];
 int32_t interval, start_frame;
 uint32_t flags, descriptors;
};
struct ws73_mon_get { struct ws73_mon_header *header; void *data; size_t capacity; };
struct ws73_mon_stats { uint32_t queued, dropped; };
_Static_assert(sizeof(struct ws73_mon_header) == 64, "usbmon ABI size");

static int ws73_mon_open(void)
{
 unsigned int major, minor;
 FILE *file = fopen("/sys/class/usbmon/usbmon0/dev", "r");
 if (!file) return -1;
 int n = fscanf(file, "%u:%u", &major, &minor);
 fclose(file);
 if (n != 2 || mknod("/dev/ws73-usbmon", S_IFCHR | 0600, makedev(major, minor))) return -1;
 int fd = open("/dev/ws73-usbmon", O_RDONLY | O_NONBLOCK);
 if (fd >= 0 && ioctl(fd, _IO(0x92, 4), 1024 * 1024)) { close(fd); return -1; }
 return fd;
}

static int ws73_mon_drain(int fd)
{
 unsigned int events = 0, consumed = 0;
 /* Give the independent text and kernel readers a turn during bursts. */
 while (consumed < 16) {
  struct ws73_mon_header header = {};
  unsigned char data[512];
  struct ws73_mon_get get = { &header, data, sizeof(data) };
  if (ioctl(fd, _IOW(0x92, 10, struct ws73_mon_get), &get)) {
   if (errno == EAGAIN || errno == EINTR) break;
   return -1;
  }
  consumed++;
  /* Enumeration/control traffic is captured by the existing text recorder. */
  if (header.transfer != 3 && header.transfer != 1) continue;
  size_t copied = header.captured < sizeof(data) ? header.captured : sizeof(data);
  char hex[sizeof(data) * 2 + 1];
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < copied; i++) {
   hex[i * 2] = digits[data[i] >> 4];
   hex[i * 2 + 1] = digits[data[i] & 15];
  }
  hex[copied * 2] = 0;
  printf("WS73_MON id=%016llx event=%c transfer=%u bus=%u device=%u ep=%02x status=%d length=%u captured=%u copied=%zu time=%lld.%06d data=%s\n",
         (unsigned long long)header.id, header.type, header.transfer, header.bus, header.device,
         header.endpoint, header.status, header.length, header.captured, copied,
         (long long)header.seconds, header.microseconds, hex);
  events++;
 }
 struct ws73_mon_stats stats;
 if (ioctl(fd, _IOR(0x92, 3, struct ws73_mon_stats), &stats)) return -1;
 if (events || stats.dropped)
  printf("WS73_MON_STATS queued=%u dropped=%u\n", stats.queued, stats.dropped);
 return stats.dropped ? -1 : 0;
}
