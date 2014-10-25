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

#ifndef NETINFO_H_
#define NETINFO_H_

#include <defs.h>
#include <list.h>
#include <cpumask.h>

#include <tsobj.h>

#include <hiobject.h>

#define HI_NET_DEV_ADDR_STRLEN		48
#define HI_NET_IPv4_STRLEN			16
#define HI_NET_IPv6_STRLEN			42

typedef enum hi_net_device_type {
	HI_NET_UNKNOWN,
	HI_NET_ETHERNET,
	HI_NET_X25,
	HI_NET_TOKEN_RING,
	HI_NET_WIRELESS,
	HI_NET_INFINIBAND,
	HI_NET_POINT_TO_POINT,
	HI_NET_TUNNEL,
	HI_NET_VIRTUAL,

	HI_NET_DEV_MAX
} hi_net_device_type_t;

typedef enum hi_net_device_duplex {
	HI_NET_NO_LINK,
	HI_NET_SIMPLEX,
	HI_NET_HALF_DUPLEX,
	HI_NET_FULL_DUPLEX
} hi_net_device_duplex_t;

typedef struct hi_net_device {
	hi_net_device_type_t 	type;
	long 					speed;
	hi_net_device_duplex_t	duplex;
	int					 	mtu;

	char					address[HI_NET_DEV_ADDR_STRLEN];
} hi_net_device_t;

typedef struct hi_net_address_flags {
	boolean_t	up;
	boolean_t	running;
	boolean_t	dynamic;
	/* For Solaris IPMP */
	boolean_t	nofailover;
	boolean_t 	deprecated;
} hi_net_address_flags_t;

typedef struct hi_net_ipv4_address {
	hi_net_address_flags_t flags;
	char address[HI_NET_IPv4_STRLEN];
	char netmask[HI_NET_IPv4_STRLEN];
} hi_net_ipv4_address_t;

typedef struct hi_net_ipv6_address {
	hi_net_address_flags_t flags;
	char address[HI_NET_IPv6_STRLEN];
	char netmask[HI_NET_IPv6_STRLEN];
} hi_net_ipv6_address_t;

typedef enum hi_net_object_type {
	HI_NET_LOOPBACK,
	HI_NET_DEVICE,
	HI_NET_VLAN,
	HI_NET_BRIDGE,
	HI_NET_BOND,
	HI_NET_SWITCH,
	HI_NET_IPv4_ADDRESS,
	HI_NET_IPv6_ADDRESS,

	HI_NET_MAXIMUM
} hi_net_object_type_t;

typedef struct hi_net_object {
	hi_object_header_t		hdr;

	hi_net_object_type_t 	type;

	union {
		hi_net_device_t 	device;
		hi_net_ipv4_address_t ipv4_address;
		hi_net_ipv6_address_t ipv6_address;
		int vlan_id;
	};
} hi_net_object_t;

#define HI_NET_FROM_OBJ(object)		((hi_net_object_t*) (object))
#define HI_NET_TO_OBJ(object)		((hi_object_t*) (object))
#define HI_NET_PARENT_OBJ(object)	object->hdr.node.parent
#define HI_NET_PARENT(object)		HI_NET_FROM_OBJ(HI_NET_PARENT_OBJ(object))

PLATAPI int hi_net_probe(void);
void hi_net_dtor(hi_object_t* object);
int hi_net_init(void);
void hi_net_fini(void);

hi_net_object_t* hi_net_create(const char* name, hi_net_object_type_t type);

/**
 * Add network object to a global list */
STATIC_INLINE void hi_net_add(hi_net_object_t* netobj) {
	hi_obj_add(HI_SUBSYS_NET, HI_NET_TO_OBJ(netobj));
}

/**
 * Attach network object to parent as a slave
 *
 * @param netobj	Slave network object
 * @param parent    Parent network object
 */
STATIC_INLINE void hi_net_attach(hi_net_object_t* netobj, hi_net_object_t* parent) {
	hi_obj_attach(HI_NET_TO_OBJ(netobj), HI_NET_TO_OBJ(parent));
}

/**
 * Find network object by it's name
 * @param name - name of network object
 *
 * @return network object or NULL if it wasn't found
 * */
STATIC_INLINE hi_net_object_t* hi_net_find(const char* name) {
	return (hi_net_object_t*)	hi_obj_find(HI_SUBSYS_NET, name);
}

/**
 * Probe system's network objects (if needed) and return pointer
 * to global network object list head
 *
 * @param reprobe Probe system's network objects again
 *
 * @return pointer to head or NULL if probe failed
 */
STATIC_INLINE list_head_t* hi_net_list(boolean_t reprobe) {
	return hi_obj_list(HI_SUBSYS_NET, reprobe);
}

tsobj_node_t* tsobj_hi_net_format(struct hi_object_header* obj);

#endif /* NETINFO_H_ */
