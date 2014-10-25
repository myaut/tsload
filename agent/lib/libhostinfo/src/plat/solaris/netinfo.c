
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
#include <hitrace.h>

#include <string.h>

#include <libdladm.h>
#include <libdllink.h>
#include <plat/libdlvnic.h>
#include <kstat.h>
#include <sys/mac.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <unistd.h>
#include <arpa/inet.h>

/**
 * ### netinfo - Solaris implementation
 *
 * Like in case with libpicl, 64-bit version of libdlmgmt is broken (but in this case - partially),
 * so we do not implement minidlmgmt here. However, some calls are workarounded through kstat.
 */

static dladm_handle_t dld_handle;

#define LOOPBACK_IF			"lo0"
#define LIFC_FLAGS			LIFC_NOXMIT

static void hi_net_sol_fetch_kstat(const char *name, hi_net_device_t* netdev) {
	kstat_ctl_t		*kc;
	kstat_t			*ksp;
	kstat_named_t	*knp;

	uint32_t state;
	uint32_t duplex;

	if((kc = kstat_open()) == NULL)
		return;

	if((ksp = kstat_lookup(kc, "link", 0, name)) == NULL) {
		goto end;
	}

	if(kstat_read(kc, ksp, NULL) == -1) {
		goto end;
	}

	hi_net_dprintf("hi_net_sol_fetch_kstat: fetching kstats from link:0:%s -> %p\n",
						name, ksp);

	if ((knp = kstat_data_lookup(ksp, "ifspeed")) == NULL) {
		goto end;
	}
	netdev->speed = knp->value.l;

	if ((knp = kstat_data_lookup(ksp, "link_duplex")) == NULL) {
		goto end;
	}
	duplex = knp->value.ui32;

	if ((knp = kstat_data_lookup(ksp, "link_state")) == NULL) {
		goto end;
	}
	state = knp->value.ui32;

	if(state == LINK_STATE_UP) {
		if(duplex == LINK_DUPLEX_HALF) {
			netdev->duplex = HI_NET_HALF_DUPLEX;
		}
		else if(duplex == LINK_DUPLEX_FULL) {
			netdev->duplex = HI_NET_FULL_DUPLEX;
		}
	}

end:
	kstat_close(kc);
}

static hi_net_object_t* hi_net_sol_create_phys(const char *name, datalink_id_t link_id, uint32_t media, boolean_t is_vnic) {
	hi_net_object_t* netobj = hi_net_create(name, HI_NET_DEVICE);
	hi_net_device_t* netdev = &netobj->device;

	char propval[DLADM_PROP_VAL_MAX];
	char* valptr[1];
	uint_t valcnt = 1;

	hi_net_dprintf("hi_net_sol_create_phys: found %s link %s id %d media %d\n",
					is_vnic ? "vnic" : "phys", name, (int) link_id, (int) media);

	if(is_vnic) {
		netdev->type = HI_NET_VIRTUAL;
	}
	else {
		switch(media) {
		case DL_ETHER:
		case DL_CSMACD:
		case DL_ETH_CSMA:
		case DL_100BT:
			netdev->type = HI_NET_ETHERNET;
			break;
		case DL_TPR:
			netdev->type = HI_NET_TOKEN_RING;
			break;
		case DL_X25:
			netdev->type = HI_NET_X25;
			break;
		case DL_IPV4:
		case DL_IPV6:
		case DL_6TO4:
			netdev->type = HI_NET_TUNNEL;
			break;
		}
	}

	valptr[0] = propval;
	if(dladm_get_linkprop(dld_handle, link_id, DLADM_PROP_VAL_CURRENT,
			"mac-address", (char**) valptr, &valcnt) == DLADM_STATUS_OK) {
		/* TODO: dladm omits starting zeroes, convert this */
		strncpy(netdev->address, propval, HI_NET_DEV_ADDR_STRLEN);
	}

	if(dladm_get_linkprop(dld_handle, link_id, DLADM_PROP_VAL_CURRENT,
			"mtu", (char**) valptr, &valcnt) == DLADM_STATUS_OK) {
		netdev->mtu = strtol(propval, NULL, 10);
	}

	if(!is_vnic) {
#if 1
		hi_net_sol_fetch_kstat(name, netdev);
#else
		if(dladm_get_linkprop(dld_handle, link_id, DLADM_PROP_VAL_CURRENT,
				"duplex", (char**) valptr, &valcnt) == DLADM_STATUS_OK) {
			hi_net_dprintf("hi_net_sol_create_phys: 	duplex %s\n", propval);

			if(strcmp(propval, "full")) {
				netdev->duplex = HI_NET_FULL_DUPLEX;
			}
			else if(strcmp(propval, "half")) {
				netdev->duplex = HI_NET_HALF_DUPLEX;
			}
			else if(strcmp(propval, "unknown")) {
				netdev->duplex = HI_NET_NO_LINK;
			}
		}

		if(dladm_get_linkprop(dld_handle, link_id, DLADM_PROP_VAL_CURRENT,
				"speed", (char**) valptr, &valcnt) == DLADM_STATUS_OK) {
			netdev->speed = (unsigned long long) (strtof(propval, NULL) * 1000 * 1000);
		}
#endif
	}
	else {
		/* Actual speed of VNIC is irrelevant so we say its 2Gbit/s which
		 * looks like a fast link and shouldn't confuse in Ethernet speeds progression
		 * (100 M -> 1 G -> 10 G -> 40 G)
		 *
		 * Why 2Gbit/s? It was a bug in Citrix XenServer para-drivers for
		 * Windows, so all virtual network devices had shown that speed. */

		netdev->duplex = HI_NET_FULL_DUPLEX;
		netdev->speed = 2000000000;
	}

	hi_net_add(netobj);

	return netobj;
}

static hi_net_object_t* hi_net_sol_create_etherstub(const char *name,
			datalink_id_t link_id, uint32_t media) {
	hi_net_object_t* netobj = hi_net_create(name, HI_NET_SWITCH);

	hi_net_add(netobj);

	return netobj;
}

/* dladm_walk callback */
static int hi_net_sol_dl_walker(const char *name, void *data)
{
	hi_net_object_t* netobj = NULL;

	datalink_id_t link_id;
	datalink_class_t link_class;
	uint32_t media;

	if(dladm_name2info(dld_handle, name, &link_id, NULL,
			&link_class, &media)!= DLADM_STATUS_OK) {
		return DLADM_WALK_CONTINUE;
	}

	hi_net_dprintf("hi_net_sol_dl_walker: found link %s id %d class %x\n",
					name, (int) link_id, (unsigned) link_class);

	switch(link_class) {
	case DATALINK_CLASS_PHYS:
		netobj = hi_net_sol_create_phys(name, link_id, media, B_FALSE);
		break;
	case DATALINK_CLASS_VNIC:
		netobj = hi_net_sol_create_phys(name, link_id, media, B_TRUE);
		break;
	case DATALINK_CLASS_ETHERSTUB:
		netobj = hi_net_sol_create_etherstub(name, link_id, media);
		break;
	default:
		return DLADM_WALK_CONTINUE;
	}

	return DLADM_WALK_CONTINUE;
}

static int hi_net_sol_dl_walker_rel(const char *name, void *data)
{
	datalink_id_t link_id;
	datalink_class_t link_class;
	uint32_t media;

	char link_over[MAXLINKNAMESPECIFIER];

	hi_net_object_t* parent;
	hi_net_object_t* netobj;

	if(dladm_name2info(dld_handle, name, &link_id, NULL,
			&link_class, &media)!= DLADM_STATUS_OK) {
		return DLADM_WALK_CONTINUE;
	}

	switch(link_class) {
		case DATALINK_CLASS_VNIC:
			{
				dladm_vnic_attr_t vnic_attr;

				if (dladm_vnic_info(dld_handle, link_id, &vnic_attr,
						DLADM_OPT_ACTIVE) != DLADM_STATUS_OK) {
					return DLADM_WALK_CONTINUE;
				}

				if (dladm_datalink_id2info(dld_handle, vnic_attr.va_link_id, NULL, NULL, NULL,
						link_over, MAXLINKNAMESPECIFIER) != DLADM_STATUS_OK) {
					return DLADM_WALK_CONTINUE;
				}
			}
			break;
		default:
			return DLADM_WALK_CONTINUE;
	}

	netobj = hi_net_find(name);
	parent = hi_net_find(link_over);

	hi_net_attach(netobj, parent);

	return DLADM_WALK_CONTINUE;
}

static int hi_net_sol_getifconf(int sock, sa_family_t af, struct lifreq** lifr, int* numifs) {
	struct lifnum lifn;
	struct lifconf lifc;

	size_t buflen;
	caddr_t* buf = (caddr_t*) lifr;

	lifn.lifn_family = af;
	lifn.lifn_flags = LIFC_FLAGS;

	do {
		if (ioctl(sock, SIOCGLIFNUM, (char *) &lifn) < 0)
			goto fail;

		buflen = (lifn.lifn_count + 4) * sizeof(struct lifreq);

		if(*buf != NULL)
			mp_free(*buf);
		*buf = mp_malloc(buflen);

		lifc.lifc_family = af;
		lifc.lifc_flags = LIFC_FLAGS;
		lifc.lifc_len = buflen;
		lifc.lifc_buf = *buf;

		if (ioctl(sock, SIOCGLIFCONF, (char *) &lifc) < 0)
			goto fail;

		*numifs = lifc.lifc_len / sizeof (struct lifreq);
	} while(*numifs >= (lifn.lifn_count + 4));

	return 0;

fail:
	if(*buf)
		mp_free(*buf);
	return -1;
}

typedef struct {
	struct sockaddr_storage ifa_addr;
	struct sockaddr_storage ifa_netmask;
	uint64_t ifa_flags;
} hi_net_sol_ifa_t;

static void hi_net_sol_ifflags(hi_net_sol_ifa_t* ifa, hi_net_address_flags_t* flags) {
	if(ifa->ifa_flags & IFF_UP)
		flags->up = B_TRUE;
	if(ifa->ifa_flags & IFF_RUNNING)
		flags->running = B_TRUE;
	if(ifa->ifa_flags & IFF_DHCPRUNNING)
		flags->dynamic = B_TRUE;
	if(ifa->ifa_flags & IFF_NOFAILOVER)
		flags->nofailover = B_TRUE;
	if(ifa->ifa_flags & IFF_DEPRECATED)
		flags->deprecated = B_TRUE;
}

static void hi_net_sol_addif_4(hi_net_object_t* parent, const char* name, hi_net_sol_ifa_t* ifa) {
	hi_net_object_t* addr = hi_net_create(name, HI_NET_IPv4_ADDRESS);

	inet_ntop(AF_INET,
			  &((struct sockaddr_in*) &ifa->ifa_addr)->sin_addr,
			  addr->ipv4_address.address, HI_NET_IPv4_STRLEN);
	inet_ntop(AF_INET,
			  &((struct sockaddr_in*) &ifa->ifa_netmask)->sin_addr,
			  addr->ipv4_address.netmask, HI_NET_IPv4_STRLEN);

	hi_net_sol_ifflags(ifa, &addr->ipv4_address.flags);

	hi_net_add(addr);
	hi_net_attach(addr, parent);

	hi_net_dprintf("hi_net_sol_addif_4: attached address %s as iface %s\n",
							addr->ipv4_address.address, name);
}

static void hi_net_sol_addif_6(hi_net_object_t* parent, const char* name, hi_net_sol_ifa_t* ifa) {
	hi_net_object_t* addr = hi_net_create(name, HI_NET_IPv6_ADDRESS);

	inet_ntop(AF_INET6,
			  &((struct sockaddr_in6*) &ifa->ifa_addr)->sin6_addr,
			  addr->ipv6_address.address, HI_NET_IPv6_STRLEN);
	inet_ntop(AF_INET6,
			  &((struct sockaddr_in6*) &ifa->ifa_netmask)->sin6_addr,
			  addr->ipv6_address.netmask, HI_NET_IPv6_STRLEN);


	hi_net_sol_ifflags(ifa, &addr->ipv6_address.flags);

	hi_net_add(addr);
	hi_net_attach(addr, parent);

	hi_net_dprintf("hi_net_sol_addif_6: attached address %s as iface %s\n",
							addr->ipv6_address.address, name);
}

static int hi_net_sol_getallifs(sa_family_t af, void (*add)(hi_net_object_t* parent, const char* name, hi_net_sol_ifa_t* ifa)) {
	int sock = socket(af, SOCK_DGRAM, 0);
	struct lifreq* lifrs = NULL;
	struct lifreq* lifr;
	int numifs;
	int ifi;

	hi_net_object_t* parent;

	char name[32];
	char ifa_name[32];
	char* addr_name;

	hi_net_sol_ifa_t ifa;

	sa_family_t lifr_af;

	if(sock == -1)
		goto end;

	if(hi_net_sol_getifconf(sock, af, &lifrs, &numifs) < 0) {
		goto end;
	}

	for(ifi = 0; ifi < numifs; ++ifi) {
		lifr = lifrs + ifi;

		lifr_af = lifr->lifr_addr.ss_family;
		if (af != AF_UNSPEC && lifr_af != af)
			continue;

		hi_net_dprintf("hi_net_sol_getallifs: found iface %s [%s]\n", lifr->lifr_name,
							(af == AF_INET6) ? "inet6" : "inet");

		/* Parse lifr_name and find appropriate net object */
		strncpy(ifa_name, lifr->lifr_name, 32);
		addr_name = strchr(ifa_name, ':');

		if(addr_name == NULL) {
			addr_name = "primary";
		}
		else {
			*addr_name = '\0';
			++addr_name;
		}

		/* To ensure that primary IP address won't collide with
		 * device name, use 'primary' suffix */
		snprintf(name, 32, "%s:%s:%s",
				 (af == AF_INET)? "ip" : "ipv6",
				 ifa_name, addr_name);

		parent = hi_net_find(ifa_name);

		if(parent == NULL)
			continue;

		/* Load addresses */
		memcpy(&ifa.ifa_addr, &lifr->lifr_addr, sizeof(struct sockaddr_storage));

		if (ioctl(sock, SIOCGLIFFLAGS, (caddr_t) lifr) < 0)
			continue;
		ifa.ifa_flags = lifr->lifr_flags;

		if (ioctl(sock, SIOCGLIFNETMASK, (caddr_t) lifr) < 0)
			continue;
		memcpy(&ifa.ifa_netmask, &lifr->lifr_addr, sizeof(struct sockaddr_storage));

		add(parent, name, &ifa);
	}

	mp_free(lifrs);

end:
	close(sock);
	return 0;
}

PLATAPI int hi_net_probe(void) {
	dladm_status_t dladm_status;

	int ret = HI_PROBE_OK;

	/* Create loopback device (not provided by dladm) */
	hi_net_add(hi_net_create(LOOPBACK_IF, HI_NET_LOOPBACK));

	if ((dladm_status = dladm_open(&dld_handle)) != DLADM_STATUS_OK) {
		hi_net_dprintf("hi_net_probe: dladm open failed: %d\n", dladm_status);
		return HI_PROBE_ERROR;
	}

	if ((dladm_status = dladm_walk(hi_net_sol_dl_walker, dld_handle, NULL, DATALINK_CLASS_ALL,
			DATALINK_ANY_MEDIATYPE, DLADM_OPT_ACTIVE)) != DLADM_STATUS_OK) {
		hi_net_dprintf("hi_net_probe: dladm walk links failed: %d\n", dladm_status);
		ret = HI_PROBE_ERROR;
		goto end;
	}

	if ((dladm_status = dladm_walk(hi_net_sol_dl_walker_rel, dld_handle, NULL, DATALINK_CLASS_ALL,
			DATALINK_ANY_MEDIATYPE, DLADM_OPT_ACTIVE)) != DLADM_STATUS_OK) {
		hi_net_dprintf("hi_net_probe: dladm walk parents failed: %d\n", dladm_status);
		ret = HI_PROBE_ERROR;
		goto end;
	}

	hi_net_sol_getallifs(AF_INET, hi_net_sol_addif_4);
	hi_net_sol_getallifs(AF_INET6, hi_net_sol_addif_6);

end:
	dladm_close(dld_handle);

	return ret;
}


