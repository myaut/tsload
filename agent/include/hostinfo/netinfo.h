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

#include <tsload/defs.h>

#include <tsload/list.h>

#include <tsload/obj/obj.h>

#include <hostinfo/hiobject.h>

/**
 * @module NetInfo 
 * 
 * Gathers information about network devices plugged into system, their 
 * relationships including link aggregation, or bridges and VLANs running on
 * top of network device and IPv4 and IPv6 addresses bound to that devices.
 * 
 * We will refer to network device as __NIC__ and to address as __interface__ 
 * in subsequent documentation.
 */

#define HI_NET_DEV_ADDR_STRLEN		48
#define HI_NET_IPv4_STRLEN			16
#define HI_NET_IPv6_STRLEN			42

/**
 * Type of NIC
 */
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

/**
 * Current state of NIC link
 * 
 * @note Not all NICs support reporting of simplex or half-duplex, \
 * thus full-duplex will be used
 */
typedef enum hi_net_device_duplex {
	HI_NET_NO_LINK,
	HI_NET_SIMPLEX,
	HI_NET_HALF_DUPLEX,
	HI_NET_FULL_DUPLEX
} hi_net_device_duplex_t;

/**
 * NIC descriptor
 * 
 * @member type NIC type
 * @member speed NIC speed in bits/s
 * @member duplex link state
 * @member mtu Maximum Transfer Unit in bytes
 * @member address Hardware address for that NIC (for ethernet - MAC address). 		\
 *  			   Address is represented as a string with colon-separated bytes 	\
 *				   formatted as hexademical numbers
 */
typedef struct hi_net_device {
	hi_net_device_type_t 	type;
	long 					speed;
	hi_net_device_duplex_t	duplex;
	int					 	mtu;

	char					address[HI_NET_DEV_ADDR_STRLEN];
} hi_net_device_t;

/**
 * Interface flags
 * 
 * @note For information on `nofailover` and `deprecated` see 					\
 * 		 [ifconfig(1M)](http://docs.oracle.com/cd/E19253-01/816-5166/6mbb1kq31/)
 * 
 * @member up interface is enabled from operating system
 * @member running interface is enabled from hardware (i.e. link is present)
 * @member dynamic interface address is assigned dynamically (i.e. using DHCP)
 */
typedef struct hi_net_address_flags {
	boolean_t	up;
	boolean_t	running;
	boolean_t	dynamic;
	/* For Solaris IPMP */
	boolean_t	nofailover;
	boolean_t 	deprecated;
} hi_net_address_flags_t;

/**
 * IPv4 interface
 * 
 * `address` and `netmask` are represented as strings in _dotted decimal_ notation.
 */
typedef struct hi_net_ipv4_address {
	hi_net_address_flags_t flags;
	char address[HI_NET_IPv4_STRLEN];
	char netmask[HI_NET_IPv4_STRLEN];
} hi_net_ipv4_address_t;

/**
 * IPv4 interface
 * 
 * `address` and `netmask` are represented as strings in RFC5952 format 
 * 
 * @note `inet_ntop()` and similiar functions are used to convert IPv6 address to string
 */
typedef struct hi_net_ipv6_address {
	hi_net_address_flags_t flags;
	char address[HI_NET_IPv6_STRLEN];
	char netmask[HI_NET_IPv6_STRLEN];
} hi_net_ipv6_address_t;

/**
 * Type of NetInfo object
 * 
 * @value	HI_NET_LOOPBACK	loopback NIC
 * @value	HI_NET_DEVICE	any non-loopback NIC
 * @value	HI_NET_VLAN		VLAN interface that is working on top of the 	\
 * 							NIC and adds tag to it like in 802.1q standard.
 * @value 	HI_NET_BRIDGE	Virtual Bridge
 * @value 	HI_NET_BOND		bond - aggregation of two or more NICs acting as \
 * 							a single entity
 * @value	HI_NET_SWITCH	virtual switch working on top of NIC
 * @value	HI_NET_IPv4_ADDRESS		IPv4 interface
 * @value	HI_NET_IPv6_ADDRESS		IPv6 interface
 */
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

/**
 * NetInfo descriptor
 * 
 * @member hdr			HIObject header
 * @member n_net_name	alias for `hdr.name` - name of NetInfo object
 * @member type			type of descriptor
 * @member device		for `HI_NET_DEVICE` - device descriptor
 * @member ipv4_address for `HI_NET_IPv4_ADDRESS` - IPv4 address descriptor
 * @member ipv6_address for `HI_NET_IPv6_ADDRESS` - IPv6 address descriptor
 * @member vlan_id		for `HI_NET_VLAN` - vlan number
 */
typedef struct hi_net_object {
	hi_object_header_t		hdr;
#define n_net_name			hdr.name
	
	hi_net_object_type_t 	type;

	union {
		hi_net_device_t 	device;
		hi_net_ipv4_address_t ipv4_address;
		hi_net_ipv6_address_t ipv6_address;
		int vlan_id;
	};
} hi_net_object_t;

/**
 * Conversion macros
 */
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
