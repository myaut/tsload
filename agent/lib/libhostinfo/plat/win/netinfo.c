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

#include <tsload/mempool.h>
#include <tsload/autostring.h>

#include <hostinfo/netinfo.h>

#include <hitrace.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>

/**
 * ### Windows
 *
 * Fetch information about network objects using GetAdaptersAddresses
 * 
 * __NOTE__: Doesn't support WMI because latter doen't provide MMU, also 
 * does not have support for teaming, VLAN and other virtual NICs.
 * 
 * __NOTE__: To distinguish interface address and NIC name, it adds id
 * as an end to NIC name, i.e. "Local Area Connection #1:0"
 */

 /*
 * TODO: Implement WMI alternative (it does not support MTU sic!)
 * TODO: Support for teaming, VLAN, etc.
 */

static void create_ip_netmask(uint8_t* netmask, int length) {
	uint8_t* p = netmask;

	while(length > 8) {
		*p++ = 0xFF;
		length -= 8;
	}

	length = 8 - length;
	*p++ = (~((1 << length) - 1)) & 0xFF;
}

void hi_win_net_create_address(hi_net_object_t* netobj, PIP_ADAPTER_ADDRESSES address,
				PIP_ADAPTER_UNICAST_ADDRESS unicast_address, int addr_id) {
	size_t name_len = 2 * wcslen(address->FriendlyName);
	char* name = mp_malloc(name_len);
	char* addr_name = NULL;

	int family;

	LPSOCKADDR sock_address = unicast_address->Address.lpSockaddr;

	hi_net_object_t* addrobj;
	hi_net_address_flags_t* flags;

	/* Pick name */
	WideCharToMultiByte(CP_UTF8, 0, address->FriendlyName, -1,
						name, name_len, NULL, NULL);

	hi_net_dprintf("hi_win_net_create_address: adapter name: %s, address name: %s\n", netobj->hdr.name, addr_name);

	family = sock_address->sa_family;

	if(family == AF_INET) {
		uint8_t netmask[4] = { 0, 0, 0, 0 };
		create_ip_netmask(netmask, unicast_address->OnLinkPrefixLength);

		aas_printf(&addr_name, "%s:%d", name, addr_id);
		addrobj = hi_net_create(addr_name, HI_NET_IPv4_ADDRESS);

		InetNtop(family, &(((struct sockaddr_in*) sock_address)->sin_addr),
				 addrobj->ipv4_address.address, HI_NET_IPv4_STRLEN);
		InetNtop(family, netmask,
				 addrobj->ipv4_address.netmask, HI_NET_IPv4_STRLEN);

		flags = &addrobj->ipv4_address.flags;
	}
	else if(family == AF_INET6) {
		uint16_t netmask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		create_ip_netmask(netmask, unicast_address->OnLinkPrefixLength);

		aas_printf(&addr_name, "%s:%d", name, addr_id);
		addrobj = hi_net_create(addr_name, HI_NET_IPv6_ADDRESS);

		InetNtop(family, &(((struct sockaddr_in6*) sock_address)->sin6_addr),
				 addrobj->ipv6_address.address, HI_NET_IPv6_STRLEN);
		InetNtop(family, netmask,
				 addrobj->ipv6_address.netmask, HI_NET_IPv6_STRLEN);

		flags = &addrobj->ipv6_address.flags;
	}
	else {
		mp_free(name);
		return;
	}

	if(address->OperStatus == IfOperStatusUp) {
		flags->up = flags->running = B_TRUE;
	}
	else {
		flags->up = flags->running = B_FALSE;
	}

	flags->dynamic = TO_BOOLEAN(address->Flags & IP_ADAPTER_DHCP_ENABLED);

	hi_net_add(addrobj);
	hi_net_attach(addrobj, netobj);

	mp_free(name);
	aas_free(&addr_name);
}

void hi_win_net_create_adapter(PIP_ADAPTER_ADDRESSES address) {
	size_t name_len = 2 * wcslen(address->Description);
	char* name = mp_malloc(name_len);

	int i;
	size_t len = 0;
	int ret;

	hi_net_object_t* netobj = NULL;
	hi_net_device_t* netdev;

	PIP_ADAPTER_UNICAST_ADDRESS unicast_address;

	WideCharToMultiByte(CP_UTF8, 0, address->Description, -1,
						name, name_len, NULL, NULL);

	hi_net_dprintf("hi_win_net_probe_adresses: adapter name: %s\n", name);

	if(address->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
		netobj = hi_net_create(name, HI_NET_LOOPBACK);
		hi_net_add(netobj);

		return;
	}

	netobj = hi_net_create(name, HI_NET_DEVICE);
	netdev = &netobj->device;

	/* Convert type of device */
	switch(address->IfType) {
	case IF_TYPE_ETHERNET_CSMACD:
		netdev->type = HI_NET_ETHERNET;
		break;
	case IF_TYPE_ISO88025_TOKENRING:
		netdev->type = HI_NET_TOKEN_RING;
		break;
	case IF_TYPE_PPP:
		netdev->type = HI_NET_POINT_TO_POINT;
		break;
	case IF_TYPE_IEEE80211:
		netdev->type = HI_NET_WIRELESS;
		break;
	}
	if(address->TunnelType != TUNNEL_TYPE_NONE) {
		netdev->type = HI_NET_TUNNEL;
	}

	/* Pick link settings */
	if(address->OperStatus == IfOperStatusUp) {
		netdev->duplex = HI_NET_FULL_DUPLEX;
		netdev->speed = max(address->ReceiveLinkSpeed, address->TransmitLinkSpeed);
	}
	else {
		netdev->duplex = HI_NET_NO_LINK;
	}
	netdev->mtu = address->Mtu;

	/* Convert MAC address to string */
	for(i = 0; i < address->PhysicalAddressLength && len < HI_NET_DEV_ADDR_STRLEN; ++i) {
		ret = snprintf(netdev->address + len, HI_NET_DEV_ADDR_STRLEN - len,
					   "%02x:", address->PhysicalAddress[i]);

		if(ret < 0)
			break;

		len += ret;
	}

	netdev->address[len - 1] = '\0';

	hi_net_add(netobj);

	/* Now handle addresses */
	i = 0;
	unicast_address = address->FirstUnicastAddress;

	while(unicast_address != NULL) {
		hi_win_net_create_address(netobj, address, unicast_address, i);

		unicast_address = unicast_address->Next;
		++i;
	}
}

PLATAPI int hi_net_probe(void) {
	ULONG buf_len = 15 * SZ_KB;
	DWORD ret;
	int tries = 4;

	PIP_ADAPTER_ADDRESSES hi_win_adapter_addresses = NULL;
	PIP_ADAPTER_ADDRESSES address;

	/* GetAdaptersAddresses */
	do {
		hi_win_adapter_addresses = mp_malloc(buf_len);

		ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
										GAA_FLAG_SKIP_DNS_SERVER ,
								   NULL, hi_win_adapter_addresses, &buf_len);

		if(ret == NO_ERROR) {
			break;
		}
		else {
			mp_free(hi_win_adapter_addresses);
			hi_win_adapter_addresses = NULL;
		}

		if(ret != ERROR_BUFFER_OVERFLOW) {
			return HI_PROBE_ERROR;
		}

		--tries;
	} while(tries > 0 && ret == ERROR_BUFFER_OVERFLOW);

	address = hi_win_adapter_addresses;

	while(address) {
		hi_win_net_create_adapter(address);
		address = address->Next;
	}

	return HI_PROBE_OK;
}