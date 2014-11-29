/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <tsload/defs.h>

#include <tsload/pathutil.h>
#include <tsload/readlink.h>
#include <tsload/autostring.h>
#include <tsload/posixdecl.h>

#include <hostinfo/netinfo.h>
#include <hostinfo/plat/sysfs.h>

#include <hitrace.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>


/**
 * ### NetInfo (Linux)
 *
 * Fetch information about network objects from Linux
 * 	- Uses /sys/class/net to walk over network devices (both physical and virtual)
 * 	- Uses getifaddrs() to fetch information on IP addresses
 *
 * 	TODO: OpenVSwitch
 */

#define SYS_NET_PATH		"/sys/class/net"

static long hi_linux_get_vlanid(const char* name, char** dev_name_ptr) {
	char* p;

	aas_copy(dev_name_ptr, name);
	p = strchr(*dev_name_ptr, '.');

	if(p == NULL) {
		return 0;
	}

	/* Split name <dev>.<vlanid> into dev and vlanid */
	*p = '\0';
	++p;

	return strtol(p, NULL, 10);
}

static void hi_linux_net_proc_netdev(const char* name, const char* net_path, unsigned type) {
	hi_net_object_t* netobj = hi_net_create(name, HI_NET_DEVICE);
	hi_net_device_t* netdev = &netobj->device;

	char duplex[32];
	char address[64];
	char* p;
	int i;
	int ret;
	unsigned carrier;

	/* Determine type of device */
	hi_net_dprintf("hi_linux_net_proc_netdev: 		--> device (type: %d)\n", type);

	switch(type) {
	case ARPHRD_ETHER:
	case ARPHRD_EETHER:
	case ARPHRD_IEEE802:
		netdev->type = HI_NET_ETHERNET;
		break;
	case ARPHRD_AX25:
	case ARPHRD_X25:
	case ARPHRD_HWX25:
		netdev->type = HI_NET_X25;
		break;
	case ARPHRD_PRONET:
		netdev->type = HI_NET_TOKEN_RING;
		break;
	case ARPHRD_PPP:
		netdev->type = HI_NET_POINT_TO_POINT;
		break;
	case ARPHRD_TUNNEL:
	case ARPHRD_TUNNEL6:
		netdev->type = HI_NET_TUNNEL;
		break;
	case ARPHRD_IEEE80211:
	case ARPHRD_IEEE80211_PRISM:
	case ARPHRD_IEEE80211_RADIOTAP:
		netdev->type = HI_NET_WIRELESS;
		break;
	}

	/* Read parameters of device */
	netdev->mtu = (unsigned) hi_linux_sysfs_readuint(SYS_NET_PATH, name, "mtu", 0);

	carrier = (unsigned) hi_linux_sysfs_readuint(SYS_NET_PATH, name, "carrier", 0);
	if(carrier == 1) {
		netdev->speed = hi_linux_sysfs_readuint(SYS_NET_PATH, name, "speed", 0);
		netdev->speed *= 1000 * 1000;

		if(hi_linux_sysfs_readstr(SYS_NET_PATH, name, "duplex",
								  duplex, 32) == HI_LINUX_SYSFS_OK) {
			if(strncmp(duplex, "full", 4) == 0) {
				netdev->duplex = HI_NET_FULL_DUPLEX;
			}
			else if(strncmp(duplex, "half", 4) == 0) {
				netdev->duplex = HI_NET_HALF_DUPLEX;
			}
		}
	}

	/* Parse MAC address */
	if(hi_linux_sysfs_readstr(SYS_NET_PATH, name, "address",
							  netdev->address, 64) != HI_LINUX_SYSFS_OK) {
		netdev->address[0] = '\0';
	}
	else {
		hi_linux_sysfs_fixstr2(netdev->address);
	}

	hi_net_add(netobj);
}

static void hi_linux_net_proc(const char* name, void* arg) {
	char net_path[256];
	char tmp_path[256];

	struct stat s;
	unsigned type = 0;

	char* vlan_name;
	long vlan_id;

	hi_net_object_t* netobj = NULL;

	path_join(net_path, 256, SYS_NET_PATH, name, NULL);

	if(stat(net_path, &s) == -1 || !S_ISDIR(s.st_mode)) {
		return;
	}

	hi_dsk_dprintf("hi_linux_net_proc: Probing %s (%s)\n", name, net_path);

	path_join(tmp_path, 256, net_path, "bonding", NULL);
	if(stat(tmp_path, &s) != -1 && S_ISDIR(s.st_mode)) {
		netobj = hi_net_create(name, HI_NET_BOND);
		hi_net_add(netobj);

		hi_net_dprintf("hi_linux_net_proc: 		--> bond\n");

		return;
	}

	path_join(tmp_path, 256, net_path, "bridge", NULL);
	if(stat(tmp_path, &s) != -1 && S_ISDIR(s.st_mode)) {
		netobj = hi_net_create(name, HI_NET_BRIDGE);
		hi_net_add(netobj);

		hi_net_dprintf("hi_linux_net_proc: 		--> bridge\n");

		return;
	}

	type = (unsigned) hi_linux_sysfs_readuint(SYS_NET_PATH, name, "type", ARPHRD_VOID);

	path_join(tmp_path, 256, net_path, "device", NULL);
	if(stat(tmp_path, &s) != -1 && S_ISDIR(s.st_mode)) {
		hi_linux_net_proc_netdev(name, net_path, type);
		return;
	}

	if(type == ARPHRD_LOOPBACK) {
		netobj = hi_net_create(name, HI_NET_LOOPBACK);
		hi_net_add(netobj);

		hi_net_dprintf("hi_linux_net_proc: 		--> loopback\n");

		return;
	}

	/* May be it is VLAN? Linux 8021q do not support sysfs (sic!), so let's play guessing game! */
	vlan_id = hi_linux_get_vlanid(name, &vlan_name);
	if(vlan_id != 0) {
		netobj = hi_net_create(vlan_name, HI_NET_VLAN);
		netobj->vlan_id = vlan_id;

		aas_free(&vlan_name);

		hi_net_add(netobj);

		hi_net_dprintf("hi_linux_net_proc: 		--> vlan %ld\n", vlan_id);

		return;
	}
	aas_free(&vlan_name);

	/* Should not be reached */
	hi_net_dprintf("hi_linux_net_proc: Unknown network object!\n");
}

static void hi_linux_net_addr_probe(void) {
	struct ifaddrs *ifaddr, *ifa;
	int err;
	int family;

	hi_net_object_t* parent = NULL;
	hi_net_object_t* addr = NULL;
	hi_net_address_flags_t* flags = NULL;

	char* name;
	char* ifa_name;
	char* addr_name;

	if (getifaddrs(&ifaddr) == -1) {
		hi_net_dprintf("hi_linux_net_addr_probe: getifaddrs() failed, errno: %d\n", errno);
		return;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		if(family != AF_INET && family != AF_INET6)
			continue;

		/* Let's find device this IP address is attached to */
		aas_init(&name);
		aas_init(&addr_name);
		aas_init(&ifa_name);

		aas_copy(&ifa_name, ifa->ifa_name);
		aas_copy(&addr_name, strchr(ifa_name, ':'));

		if(addr_name == NULL) {
			addr_name = "primary";
		}

		/* To ensure that primary IP address won't collide with
		 * device name, use 'primary' suffix */
		aas_printf(&name, "%s:%s:%s",
				   (family == AF_INET)? "ip" : "ipv6",
				   ifa_name, addr_name);

		parent = hi_net_find(ifa_name);

		if(parent == NULL) {
			hi_net_dprintf("hi_linux_net_addr_probe: Couldn't find device %s\n", ifa_name);
			continue;
		}

		/* Let's create device
		 * XXX: does endianess conversion needed here? */
		if(family == AF_INET) {
			addr = hi_net_create(name, HI_NET_IPv4_ADDRESS);

			inet_ntop(AF_INET,
				      &((struct sockaddr_in*) ifa->ifa_addr)->sin_addr,
				      addr->ipv4_address.address, HI_NET_IPv4_STRLEN);
			inet_ntop(AF_INET,
					  &((struct sockaddr_in*) ifa->ifa_netmask)->sin_addr,
					  addr->ipv4_address.netmask, HI_NET_IPv4_STRLEN);

			flags = &addr->ipv4_address.flags;
		}
		else if(family == AF_INET6) {
			addr = hi_net_create(name, HI_NET_IPv6_ADDRESS);

			inet_ntop(AF_INET6,
					  &((struct sockaddr_in6*) ifa->ifa_addr)->sin6_addr,
					  addr->ipv6_address.address, HI_NET_IPv6_STRLEN);
			inet_ntop(AF_INET6,
					  &((struct sockaddr_in6*) ifa->ifa_netmask)->sin6_addr,
					  addr->ipv6_address.netmask, HI_NET_IPv6_STRLEN);

			flags = &addr->ipv6_address.flags;
		}

		hi_net_dprintf("hi_linux_net_addr_probe: Created address %s\n", name);

		if(ifa->ifa_flags & IFF_UP)
			flags->up = B_TRUE;
		if(ifa->ifa_flags & IFF_RUNNING)
			flags->running = B_TRUE;
		if(ifa->ifa_flags & IFF_DYNAMIC)
			flags->dynamic = B_TRUE;

		hi_net_add(addr);
		hi_net_attach(addr, parent);

		aas_free(&name);
		aas_free(&ifa_name);
		aas_free(&addr_name);
	}

	freeifaddrs(ifaddr);
}

static void hi_linux_net_attach_impl(const char* child_name, const char* parent_name) {
	hi_net_object_t* parent = NULL;
	hi_net_object_t* child = NULL;

	hi_net_dprintf("hi_linux_net_attach_impl:		%s --> %s\n", child_name, parent_name);

	child = hi_net_find(child_name);
	parent = hi_net_find(parent_name);

	if(parent != NULL && child != NULL) {
		hi_net_attach(child, parent);
	}
}

static void hi_linux_net_attach_link(const char* path, const char* name) {
	char link_path[256];
	const char* basename;

	struct stat s;

	path_split_iter_t iter;

	hi_net_dprintf("hi_linux_net_attach_impl: Attaching: %s\n", path);

	if(lstat(path, &s) != -1 && S_ISLNK(s.st_mode)) {
		plat_readlink(path, link_path, 256);
		basename = path_basename(&iter, link_path);

		hi_linux_net_attach_impl(name, basename);
	}
}

static void hi_linux_net_attach(const char* name, void* arg) {
	char net_path[256];
	char tmp_path[256];

	char* vlan_dev_name;
	long vlan_id;

	path_join(net_path, 256, SYS_NET_PATH, name, NULL);

	path_join(tmp_path, 256, net_path, "brport", "bridge", NULL);
	hi_linux_net_attach_link(tmp_path, name);

	path_join(tmp_path, 256, net_path, "master", NULL);
	hi_linux_net_attach_link(tmp_path, name);

	vlan_id = hi_linux_get_vlanid(name, &vlan_dev_name);
	if(vlan_id != 0) {
		hi_linux_net_attach_impl(name, vlan_dev_name);
	}
	aas_free(&vlan_dev_name);

	return;
}

PLATAPI int hi_net_probe(void) {
	int ret;

	ret = hi_linux_sysfs_walk(SYS_NET_PATH, hi_linux_net_proc, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;

	ret = hi_linux_sysfs_walk(SYS_NET_PATH, hi_linux_net_attach, NULL);
	if(ret != HI_LINUX_SYSFS_OK)
		return HI_PROBE_ERROR;

	hi_linux_net_addr_probe();

	return HI_PROBE_OK;
}
