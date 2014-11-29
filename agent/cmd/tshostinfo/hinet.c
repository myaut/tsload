/*
 * hinet.c
 *
 *  Created on: Sep 14, 2014
 *      Author: myaut
 */

#include <tsload/defs.h>

#include <hostinfo/netinfo.h>

#include <hiprint.h>

#include <stdio.h>


#define SPEED_KBIT_S		1000
#define SPEED_MBIT_S		1000000
#define SPEED_GBIT_S		1000000000

#ifdef PLAT_WIN
#define HI_NET_BASE_FORMAT "%-7s %-40s "
#else
#define HI_NET_BASE_FORMAT "%-7s %-12s "
#endif

void print_net_object(int flags, int indent, hi_net_object_t* netobj, boolean_t ip_addr, boolean_t verbose_dev);

static const char* hi_net_device_types[HI_NET_DEV_MAX] = {
	"??",
	"[eth]",
	"[x25]",
	"[tr]",
	"[vlan]",
	"[ib]",
	"[ppp]",
	"[tun]",
	"[vif]"
};

static const char hi_net_duplex[4] = {
	'-', 'S', 'H', 'F'
};

void print_net_children(int indent, int flags, hi_net_object_t* netobj) {
	/* Print children */
	hi_object_child_t* child_obj;
	hi_net_object_t* child;

	int tmp;

	if(!list_empty(&netobj->hdr.children)) {
		hi_for_each_child(child_obj, &netobj->hdr) {
			child = HI_NET_FROM_OBJ(child_obj->object);

			tmp = indent;
			while(--tmp)
				fputs("    ", stdout);

			print_net_object(flags, indent + 1, child, B_TRUE, B_FALSE);
		}
	}
}

void print_net_device_info(int flags, hi_net_object_t* netobj) {
	hi_net_device_t* netdev = &netobj->device;

	char speed_str[64];
	unsigned long speed = netdev->speed;

	if(speed == 0) {
		speed_str[0] = '\0';
	}
	else if((speed % SPEED_GBIT_S) == 0) {
		snprintf(speed_str, 64, "%ldGbit/s", speed / SPEED_GBIT_S);
	}
	else if((speed % SPEED_MBIT_S) == 0) {
		snprintf(speed_str, 64, "%ldMbit/s", speed / SPEED_MBIT_S);
	}
	else if((speed % SPEED_KBIT_S) == 0) {
		snprintf(speed_str, 64, "%ldkbit/s", speed / SPEED_KBIT_S);
	}
	else {
		snprintf(speed_str, 64, "%ldbit/s", speed);
	}

	printf(HI_NET_BASE_FORMAT "%-18s %-8s %c %-6d\n",
			hi_net_device_types[netdev->type], netobj->hdr.name, netdev->address,
		    speed_str, hi_net_duplex[netdev->duplex], netdev->mtu);
}

#define PRINT_FLAG(flag, str)			\
		if(flag) {						\
			if(!first) {				\
				fputc(',', stdout);		\
			}							\
			else {						\
				fputc('<', stdout);		\
				first = B_FALSE;		\
			}							\
			fputs(str, stdout);			\
		}

void print_addr_flags(hi_net_address_flags_t* flags) {
	boolean_t first = B_TRUE;

	PRINT_FLAG(flags->up, "UP");
	PRINT_FLAG(flags->running, "RUNNING");
	PRINT_FLAG(flags->dynamic, "DYNAMIC");
	PRINT_FLAG(flags->deprecated, "DEPRECATED");
	PRINT_FLAG(flags->nofailover, "NOFAILOVER");

	if(!first) {
		fputc('>', stdout);
	}
}

void print_net_object(int flags, int indent, hi_net_object_t* netobj, boolean_t ip_addr, boolean_t verbose_dev) {
	switch(netobj->type) {
	case HI_NET_DEVICE:
		if(verbose_dev) {
			print_net_device_info(flags, netobj);
		}
		else {
			printf(HI_NET_BASE_FORMAT "\n",
				   hi_net_device_types[netobj->device.type], netobj->hdr.name);
		}
		break;
	case HI_NET_VLAN:
		printf(HI_NET_BASE_FORMAT "%d\n", "[vlan]", netobj->hdr.name, netobj->vlan_id);
		break;
	case HI_NET_LOOPBACK:
		printf(HI_NET_BASE_FORMAT "\n", "[lo]", netobj->hdr.name);
		break;
	case HI_NET_BOND:
		printf(HI_NET_BASE_FORMAT "\n", "[bond]", netobj->hdr.name);
		break;
	case HI_NET_BRIDGE:
		printf(HI_NET_BASE_FORMAT "\n", "[br]", netobj->hdr.name);
		break;
	case HI_NET_SWITCH:
		printf(HI_NET_BASE_FORMAT "\n", "[vsw]", netobj->hdr.name);
		break;
	}

	if((flags & INFO_XDATA) && verbose_dev) {
		print_net_children(flags, indent, netobj);
	}

	if(!ip_addr)
		return;

	switch(netobj->type) {
		case HI_NET_IPv4_ADDRESS:
			printf(HI_NET_BASE_FORMAT "\n", "[ip]", netobj->hdr.name);
			printf("\tinet %s netmask %s ", netobj->ipv4_address.address,
					netobj->ipv4_address.netmask);
			print_addr_flags(&netobj->ipv4_address.flags);
			fputc('\n', stdout);
			break;
		case HI_NET_IPv6_ADDRESS:
			printf(HI_NET_BASE_FORMAT "\n", "[ipv6]", netobj->hdr.name);
			printf("\tinet6 %s netmask %s ", netobj->ipv6_address.address,
					netobj->ipv6_address.netmask);
			print_addr_flags(&netobj->ipv6_address.flags);
			fputc('\n', stdout);
			break;
	}

}

int print_net_info(int flags) {
	list_head_t* net_list;
	hi_net_object_t* netobj;
	hi_object_t* object;

	net_list = hi_net_list(B_FALSE);

	if(net_list == NULL)
		return INFO_ERROR;

	print_header(flags, "NETWORK INFORMATION");

	if(flags & INFO_LEGEND) {
		printf(HI_NET_BASE_FORMAT "%-18s %-8s %c %-6s\n", "TYPE", "NAME", "ADDRESS",
				"SPEED", '*', "MTU");
	}

	hi_for_each_object(object, net_list) {
		netobj = HI_NET_FROM_OBJ(object);

		print_net_object(flags, 0, netobj, B_FALSE, B_TRUE);
	}

	if(flags & INFO_LEGEND) {
		puts("\n* DUPLEX: '-' no link, 'S' - simplex, 'H' - half, 'F' - full ");
	}

	return INFO_OK;
}
