/*
 *
 * (C) 2015 - ntop.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ntop_includes.h"
#ifdef __linux__
#include <linux/if_tun.h>
#endif
#include <unistd.h>

/* ********************************************* */

PacketDumperTuntap::PacketDumperTuntap(NetworkInterface *i) {
  char *name = i->get_name();

  int ret = openTap(NULL, DUMP_MTU);
  if(ret < 0) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Opening tap (%s) failed", name);
    init_ok = false;
  } else {
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "%s: dumping packets on tap interface %s", name, dev_name);
    init_ok = true;
    num_dumped_packets = 0;
  }
}

/* ********************************************* */

PacketDumperTuntap::~PacketDumperTuntap() {
  closeTap();
}

/* ********************************************* */

char* macaddr_str (const char *mac, char *buf) {
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0] & 0xFF, mac[1] & 0xFF, mac[2] & 0xFF,
          mac[3] & 0xFF, mac[4] & 0xFF, mac[5] & 0xFF);
  return(buf);
}

#ifdef linux
void PacketDumperTuntap::readMac(char *ifname, dump_mac_t mac_addr) {
  int _sock, res;
  struct ifreq ifr;
  macstr_t mac_addr_buf;

  memset (&ifr, 0, sizeof(struct ifreq));

  /* Dummy socket, just to make ioctls with */
  _sock = socket(PF_INET, SOCK_DGRAM, 0);
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
  res = ioctl(_sock, SIOCGIFHWADDR, &ifr);
  if(res < 0) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Cannot get hw addr");
  } else
    memcpy(mac_addr, ifr.ifr_ifru.ifru_hwaddr.sa_data, 6);

  ntop->getTrace()->traceEvent(TRACE_NORMAL, "Interface %s has MAC %s",
			       ifname,
			       macaddr_str((char *)mac_addr, mac_addr_buf));
  close(_sock);
}
#else
void PacketDumperTuntap::readMac(char *ifname, dump_mac_t mac_addr) {
}
#endif

/* ********************************************* */

#ifdef linux
#define LINUX_SYSTEMCMD_SIZE 128

int PacketDumperTuntap::openTap(char *dev, /* user-definable interface name, eg. edge0 */ int mtu) {
  char *tuntap_device = strdup("/dev/net/tun");
  char buf[LINUX_SYSTEMCMD_SIZE];
  struct ifreq ifr;
  int rc;

  this->fd = open(tuntap_device, O_RDWR);
  if(this->fd < 0) {
    printf("ERROR: ioctl() [%s][%d]\n", strerror(errno), errno);
    free(tuntap_device);
    return -1;
  }
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP|IFF_NO_PI; /* Want a TAP device for layer 2 frames. */
  if(dev)
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

  rc = ioctl(this->fd, TUNSETIFF, (void *)&ifr);
  if(rc < 0) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "ioctl() [%s][%d]\n", strerror(errno), rc);
    free(tuntap_device);
    close(this->fd);
    return -1;
  }

  /* Store the device name for later reuse */
  strncpy(this->dev_name, ifr.ifr_name,
          (IFNAMSIZ < DUMP_IFNAMSIZ ? IFNAMSIZ : DUMP_IFNAMSIZ) );
  snprintf(buf, sizeof(buf), "/sbin/ifconfig %s up mtu %d",
           ifr.ifr_name, DUMP_MTU);
  system(buf);
  ntop->getTrace()->traceEvent(TRACE_INFO, "Bringing up: %s", buf);
  readMac(this->dev_name, this->mac_addr);
  free(tuntap_device);
  return(this->fd);
}
#endif

/* ********************************************* */

#ifdef __FreeBSD
#define FREEBSD_TAPDEVICE_SIZE 32
int PacketDumperTuntap::openTap(char *dev, /* user-definable interface name, eg. edge0 */ int mtu) {
  int i;
  char tap_device[FREEBSD_TAPDEVICE_SIZE];

  for (i = 0; i < 255; i++) {
    snprintf(tap_device, sizeof(tap_device), "/dev/tap%d", i);
    this->fd = open(tap_device, O_RDWR);
    if(this->fd > 0) {
      ntop->getTrace()->traceEvent(TRACE_NORMAL, "Succesfully open %s", tap_device);
      break;
    }
  }
  if(this->fd < 0) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Unable to open tap device");
    return(-1);
  }

  ntop->getTrace()->traceEvent(TRACE_NORMAL, "Interface tap%d up and running", i);
  return(this->fd);
}
#endif

/* ********************************************* */

#ifdef __APPLE__
#define OSX_TAPDEVICE_SIZE 32

int PacketDumperTuntap::openTap(char *dev, /* user-definable interface name, eg. edge0 */ int mtu) {
  int i;
  char tap_device[OSX_TAPDEVICE_SIZE];

  for (i = 0; i < 255; i++) {
    snprintf(tap_device, sizeof(tap_device), "/dev/tap%d", i);
    this->fd = open(tap_device, O_RDWR);
    if(this->fd > 0) {
      ntop->getTrace()->traceEvent(TRACE_NORMAL, "Succesfully open %s", tap_device);
      break;
    }
  }

  if(this->fd < 0) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Unable to open tap device");
    return -1;
  }

  ntop->getTrace()->traceEvent(TRACE_NORMAL, "Interface tap%d up and running", i);
  return(this->fd);
}
#endif

/* ********************************************* */

int PacketDumperTuntap::readTap(unsigned char *buf, int len) {
  if(init_ok)
    return(read(this->fd, buf, len));
  return 0;
}

int PacketDumperTuntap::writeTap(unsigned char *buf, int len,
                                 dump_reason reason, unsigned int sampling_rate) {
  if(init_ok) {
    int rate_dump_ok = reason != ATTACK || num_dumped_packets % sampling_rate == 0;
    if(!rate_dump_ok) return 0;
    num_dumped_packets++;
    return(write(this->fd, buf, len));
  }
  return 0;
}

void PacketDumperTuntap::closeTap() {
  if(init_ok)
    close(this->fd);
}

void PacketDumperTuntap::lua(lua_State *vm) {
  lua_newtable(vm);
  lua_push_int_table_entry(vm, "num_dumped_pkts", get_num_dumped_packets());

  lua_pushstring(vm, "pkt_dumper_tuntap");
  lua_insert(vm, -2);
  lua_settable(vm, -3);
}