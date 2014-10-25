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

#include <mempool.h>

#include <netinfo.h>

#include <string.h>

mp_cache_t hi_net_obj_cache;

static void hi_net_init_flags(hi_net_address_flags_t* flags) {
	flags->up = B_FALSE;
	flags->running = B_FALSE;
	flags->dynamic = B_FALSE;

	flags->nofailover = B_FALSE;
	flags->deprecated = B_FALSE;
}

hi_net_object_t* hi_net_create(const char* name, hi_net_object_type_t type) {
	hi_net_object_t* netobj = mp_cache_alloc(&hi_net_obj_cache);

	netobj->type = type;

	switch(type) {
	case HI_NET_DEVICE:
		netobj->device.type = HI_NET_UNKNOWN;
		netobj->device.speed = 0;
		netobj->device.duplex = HI_NET_NO_LINK;
		netobj->device.address[0] = '\0';
		netobj->device.mtu = 0;
		break;
	case HI_NET_VLAN:
		netobj->vlan_id = 0;
		break;
	case HI_NET_LOOPBACK:
	case HI_NET_BRIDGE:
	case HI_NET_BOND:
	case HI_NET_SWITCH:
		break;
	case HI_NET_IPv4_ADDRESS:
		hi_net_init_flags(&netobj->ipv4_address.flags);
		memset(&netobj->ipv4_address, 0, sizeof(hi_net_ipv4_address_t));
		break;
	case HI_NET_IPv6_ADDRESS:
		hi_net_init_flags(&netobj->ipv6_address.flags);
		memset(&netobj->ipv6_address, 0, sizeof(hi_net_ipv6_address_t));
		break;
	}

	hi_obj_header_init(HI_SUBSYS_NET, &netobj->hdr, name);

	return netobj;
}

void hi_net_dtor(hi_object_t* obj) {
	hi_net_object_t* netobj = HI_NET_FROM_OBJ(obj);
	mp_cache_free(&hi_net_obj_cache, netobj);
}

int hi_net_init(void) {
	mp_cache_init(&hi_net_obj_cache, hi_net_object_t);

	return 0;
}

void hi_net_fini(void) {
	mp_cache_destroy(&hi_net_obj_cache);
}

static tsobj_str_t tsobj_hi_net_device_types[HI_NET_DEV_MAX] = {
	TSOBJ_STR("Unknown"),
	TSOBJ_STR("Ethernet"),
	TSOBJ_STR("X.25"),
	TSOBJ_STR("Token Ring"),
	TSOBJ_STR("Wireless"),
	TSOBJ_STR("InfiniBand"),
	TSOBJ_STR("Point-to-Point"),
	TSOBJ_STR("Tunnel"),
};

static tsobj_str_t tsobj_hi_net_device_duplex[4] = {
	TSOBJ_STR("no-link"),
	TSOBJ_STR("simplex"),
	TSOBJ_STR("half"),
	TSOBJ_STR("duplex")
};


void tsobj_hi_net_format_addr_flags(tsobj_node_t* addr, hi_net_address_flags_t* flags) {
	tsobj_add_boolean(addr, TSOBJ_STR("up"), flags->up);
	tsobj_add_boolean(addr, TSOBJ_STR("running"), flags->running);
	tsobj_add_boolean(addr, TSOBJ_STR("dynamic"), flags->dynamic);
	tsobj_add_boolean(addr, TSOBJ_STR("nofailover"), flags->nofailover);
	tsobj_add_boolean(addr, TSOBJ_STR("deprecated"), flags->deprecated);
}

tsobj_node_t* tsobj_hi_net_format_ipv4(hi_net_ipv4_address_t* addr_obj) {
	tsobj_node_t* ipv4 = tsobj_new_node("tsload.hi.AddrIPv4");

	tsobj_add_string(ipv4, TSOBJ_STR("address"),
				     tsobj_str_create(addr_obj->address));
	tsobj_add_string(ipv4, TSOBJ_STR("netmask"),
			         tsobj_str_create(addr_obj->netmask));

	tsobj_hi_net_format_addr_flags(ipv4, &addr_obj->flags);

	return ipv4;
}

tsobj_node_t* tsobj_hi_net_format_ipv6(hi_net_ipv6_address_t* addr_obj) {
	tsobj_node_t* ipv6 = tsobj_new_node("tsload.hi.AddrIPv6");

	tsobj_add_string(ipv6, TSOBJ_STR("address"),
				   	 tsobj_str_create(addr_obj->address));
	tsobj_add_string(ipv6, TSOBJ_STR("netmask"),
				     tsobj_str_create(addr_obj->netmask));

	tsobj_hi_net_format_addr_flags(ipv6, &addr_obj->flags);

	return ipv6;
}

tsobj_node_t* tsobj_hi_net_format_device(hi_net_device_t* netdev) {
	tsobj_node_t* net = tsobj_new_node("tsload.hi.NetDevice");

	tsobj_add_string(net, TSOBJ_STR("type"),
					 tsobj_hi_net_device_types[netdev->type]);
	tsobj_add_integer(net, TSOBJ_STR("speed"), netdev->speed);
	tsobj_add_string(net, TSOBJ_STR("duplex"),
					 tsobj_hi_net_device_duplex[netdev->duplex]);
	tsobj_add_integer(net, TSOBJ_STR("mtu"), netdev->mtu);

	tsobj_add_string(net, TSOBJ_STR("address"),
					 tsobj_str_create(netdev->address));

	return net;
}

tsobj_node_t* tsobj_hi_net_format(struct hi_object_header* obj) {
	tsobj_node_t* net;
	hi_net_object_t* netobj = HI_NET_FROM_OBJ(obj);

	switch(netobj->type) {
	case HI_NET_DEVICE:
		return tsobj_hi_net_format_device(&netobj->device);
	case HI_NET_IPv4_ADDRESS:
		return tsobj_hi_net_format_ipv4(&netobj->ipv4_address);
	case HI_NET_IPv6_ADDRESS:
		return tsobj_hi_net_format_ipv6(&netobj->ipv6_address);
	case HI_NET_VLAN:
		net = tsobj_new_node("tsload.hi.NetVLAN");
		tsobj_add_integer(net, TSOBJ_STR("vlan_id"), netobj->vlan_id);
		break;
	case HI_NET_LOOPBACK:
		net = tsobj_new_node("tsload.hi.NetLoopback");
		break;
	case HI_NET_BRIDGE:
		net = tsobj_new_node("tsload.hi.NetBridge");
		break;
	case HI_NET_BOND:
		net = tsobj_new_node("tsload.hi.NetBond");
		break;
	}

	return net;
}
