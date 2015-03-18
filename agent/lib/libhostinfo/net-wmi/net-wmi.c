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
#include <hostinfo/plat/wmi.h>

#include <hitrace.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>

/**
 * ### WMI  
 * 
 * WMI-based implementation of NetInfo. Currently not supported.
 * 
 * Uses Win32_NetworkAdapter WMI table to gather information
 */

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

int hi_net_wmi_probe(void) {
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
