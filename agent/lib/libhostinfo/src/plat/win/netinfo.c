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
#include <plat/wmi.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>


/**
 * ### NetInfo (Linux)
 *
 * Fetch information about network objects using GetAdaptersAddresses
 *
 * TODO: Implement WMI alternative (it does not support MTU sic!)
 * TODO: Support for teaming, VLAN, etc.
 */

#ifndef HI_NET_WINDOWS_USE_WMI

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
	char name[HIOBJNAMELEN];
	char addr_name[HIOBJNAMELEN];

	int family;

	LPSOCKADDR sock_address = unicast_address->Address.lpSockaddr;

	hi_net_object_t* addrobj;
	hi_net_address_flags_t* flags;

	/* Pick name */
	WideCharToMultiByte(CP_UTF8, 0, address->FriendlyName, -1,
						name, HIOBJNAMELEN, NULL, NULL);
	snprintf(addr_name, HIOBJNAMELEN, "%s:%d", name, addr_id);

	hi_net_dprintf("hi_win_net_create_address: adapter name: %s, address name: %s\n", netobj->hdr.name, addr_name);

	family = sock_address->sa_family;

	if(family == AF_INET) {
		uint8_t netmask[4] = { 0, 0, 0, 0 };
		create_ip_netmask(netmask, unicast_address->OnLinkPrefixLength);

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

		addrobj = hi_net_create(addr_name, HI_NET_IPv6_ADDRESS);

		InetNtop(family, &(((struct sockaddr_in6*) sock_address)->sin6_addr),
				 addrobj->ipv6_address.address, HI_NET_IPv6_STRLEN);
		InetNtop(family, netmask,
				 addrobj->ipv6_address.netmask, HI_NET_IPv6_STRLEN);

		flags = &addrobj->ipv6_address.flags;
	}
	else {
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
}

void hi_win_net_create_adapter(PIP_ADAPTER_ADDRESSES address) {
	char name[HIOBJNAMELEN];

	int i;
	size_t len = 0;
	int ret;

	hi_net_object_t* netobj = NULL;
	hi_net_device_t* netdev;

	PIP_ADAPTER_UNICAST_ADDRESS unicast_address;

	WideCharToMultiByte(CP_UTF8, 0, address->Description, -1,
						name, HIOBJNAMELEN, NULL, NULL);

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


#else

int hi_win_net_probe_netdev(hi_wmi_t* wmi) {
	hi_net_object_t* netobj;
	hi_net_device_t* netdev;

	char name[HIOBJNAMELEN];
	char net_conn_id[256];
	char macaddress[HI_NET_DEV_ADDR_STRLEN];
	int64_t typeid;
	int64_t speed;
	boolean_t net_enabled;

	hi_wmi_iter_t iter;
	int ret = hi_wmi_query(wmi, &iter, L"WQL",
			   L"SELECT AdapterTypeID,MACAddress,Name,NetConnectionID,NetEnabled,Speed FROM Win32_NetworkAdapter");

	if(ret != HI_WMI_OK) {
		hi_net_dprintf("hi_net_probe: Failed to query Win32_NetworkAdapter\n");
		return ret;
	}

	while(hi_wmi_next(&iter)) {
		/* Fetch props from WMI */
		if(hi_wmi_get_string(&iter, L"Name", name, HIOBJNAMELEN) != HI_WMI_OK) {
			hi_net_dprintf("hi_net_probe: Failed to fetch Win32_NetworkAdapter.Name\n");
			continue;
		}

		if(hi_wmi_get_string(&iter, L"MACAddress",
				macaddress, HI_NET_DEV_ADDR_STRLEN) != HI_WMI_OK) {
			macaddress[0] = '\0';
		}

		if(hi_wmi_get_integer(&iter, L"AdapterTypeID", &typeid) != HI_WMI_OK) {
			hi_net_dprintf("hi_net_probe: Failed to fetch Win32_NetworkAdapter.AdapterTypeID for %s\n", name);
			continue;
		}

		if(hi_wmi_get_integer(&iter, L"Speed", &speed) != HI_WMI_OK) {
			speed = 0;
		}

		if(hi_wmi_get_boolean(&iter, L"NetEnabled", &net_enabled) != HI_WMI_OK) {
			net_enabled = B_FALSE;
		}

		/* Create device object */
		netobj = hi_net_create(name, HI_NET_DEVICE);
		netdev = &netobj->device;

		/* FIXME: copy mac with lower case */
		strncpy(netdev->address, macaddress, HI_NET_DEV_ADDR_STRLEN);

		/* See http://msdn.microsoft.com/en-us/library/aa394216%28v=vs.85%29.aspx */
		switch(typeid) {
		case 0:
		case 5:
			netdev->type = HI_NET_ETHERNET;
			break;
		case 1:
			netdev->type = HI_NET_TOKEN_RING;
			break;
		case 9:
		case 10:
			netdev->type = HI_NET_WIRELESS;
			break;
		}

		/* Windows doesn't distinguish duplex modes, so assume it is full-duplex */
		if(net_enabled) {
			netdev->duplex = HI_NET_FULL_DUPLEX;
			netdev->speed = speed;
		}
		else {
			netdev->duplex = HI_NET_NO_LINK;
		}

		/* TODO: Probe IP addresses here */

		hi_net_add(netobj);
	}
}

/*platapi*/ int hi_net_probe(void) {
	hi_wmi_t wmi;
	int ret = HI_PROBE_OK;

	hi_win_net_probe_adresses();

	if(hi_wmi_connect(&wmi, HI_WMI_ROOT_CIMV2) != HI_WMI_OK) {
		hi_net_dprintf("hi_net_probe: Failed to connect to WMI\n");

		return HI_PROBE_ERROR;
	}

	ret = hi_win_net_probe_netdev(&wmi);
	if(ret != HI_PROBE_OK)
		goto end;

	/* IP */

end:
	hi_wmi_disconnect(&wmi);

	return ret;
}

#endif
