
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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

#include <hostinfo/uname.h>
#include <hostinfo/plat/wmi.h>

#include <string.h>
#include <stdio.h>

#include <windows.h>


#define UNAMELEN		64
#define SYSNAMELEN		256

LIBEXPORT PLATAPIDECL(hi_get_nodename) char hi_win_nodename[MAX_COMPUTERNAME_LENGTH + 1];
LIBEXPORT PLATAPIDECL(hi_get_domainname) char hi_win_domainname[MAX_COMPUTERNAME_LENGTH + 1];

LIBEXPORT PLATAPIDECL(hi_get_os_name) char hi_win_os_name[UNAMELEN] = "Unknown Windows";
LIBEXPORT PLATAPIDECL(hi_get_os_release) char hi_win_os_release[UNAMELEN] = "?";
LIBEXPORT PLATAPIDECL(hi_get_mach) char hi_win_mach[UNAMELEN] = "?";

boolean_t hi_win_osinfo = B_FALSE;

LIBEXPORT PLATAPIDECL(hi_get_sys_name) char hi_win_sys_name[SYSNAMELEN] = "Unknown system";
boolean_t hi_win_sysname_found = B_FALSE;

/**
 * ### Windows
 * 
 * Uses GetVersionEx() and GetSystemInfo() pair of calls to get all system information
 * GetComputerName*() functions provide host name and domain
 * 
 * __NOTE__: Operating system name is deduced from `dwMajorVersion` / `dwMinorVersion`
 * so for newer Windows versions it may return "Unknown ..."
 * 
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429%28v=vs.85%29.aspx
 * */

static void hi_win_get_osinfo(void);

/* Returns operating system name and version */
PLATAPI const char* hi_get_os_name() {
	if(!hi_win_osinfo)
		hi_win_get_osinfo();

	return hi_win_os_name;
}

PLATAPI const char* hi_get_os_release() {
	if(!hi_win_osinfo)
		hi_win_get_osinfo();

	return hi_win_os_release;
}

/* Returns machine architecture */
PLATAPI const char* hi_get_mach() {
	if(!hi_win_osinfo)
		hi_win_get_osinfo();

	return hi_win_mach;
}

/* Returns nodename and domain name of current host*/
PLATAPI const char* hi_get_nodename() {
	DWORD size = 64;
	GetComputerName(hi_win_nodename, &size);

	return hi_win_nodename;
}

PLATAPI const char* hi_get_domainname() {
	DWORD size = 64;
	GetComputerNameEx(ComputerNameDnsDomain, hi_win_domainname, &size);

	return hi_win_domainname;
}

static void hi_win_get_osinfo(void) {
	SYSTEM_INFO sys_info;
	OSVERSIONINFOEX os_ver_info;

	memset(&sys_info, 0, sizeof(SYSTEM_INFO));
	memset(&os_ver_info, 0, sizeof(OSVERSIONINFOEX));

	os_ver_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if(!GetVersionEx((OSVERSIONINFO*) &os_ver_info))
		return;

	GetSystemInfo(&sys_info);

	if(os_ver_info.dwMajorVersion == 6) {
		if(os_ver_info.wProductType == VER_NT_WORKSTATION) {
			const char* desktop6[] = {"Windows Vista",
									  "Windows 7",
									  "Windows 8"};
			const char* osname = (os_ver_info.dwMinorVersion <= 2)
									? desktop6[os_ver_info.dwMinorVersion]
									: "Unknown Windows Desktop 6.x";
			strcpy(hi_win_os_name, osname);
		}
		else {
			const char* server6[] = {"Windows Server 2008",
									 "Windows Server 2008 R2",
									 "Windows Server 2012"};
			const char* osname = (os_ver_info.dwMinorVersion <= 2)
									? server6[os_ver_info.dwMinorVersion]
									: "Unknown Windows Server 6.x";
			strcpy(hi_win_os_name, osname);
		}
	}
	else if(os_ver_info.dwMajorVersion == 5) {
		switch(os_ver_info.dwMinorVersion) {
		case 0:
			strcpy(hi_win_os_name, "Windows 2000");
			break;
		case 1:
			strcpy(hi_win_os_name, "Windows XP");
			break;
		default:
			strcpy(hi_win_os_name, "Unknown Windows 5.x");
			break;
		case 2:
		{
			if(os_ver_info.wProductType == VER_NT_WORKSTATION &&
			   sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
					strcpy(hi_win_os_name, "Windows XP x64");
			}
			else if(os_ver_info.wSuiteMask & VER_SUITE_WH_SERVER) {
				strcpy(hi_win_os_name, "Windows Home Server");
			}
			else {
				if(GetSystemMetrics(SM_SERVERR2) == 0) {
					strcpy(hi_win_os_name, "Windows Server 2003");
				}
				else {
					strcpy(hi_win_os_name, "Windows Server 2003 R2");
				}
			}
		}
		}
	}

	snprintf(hi_win_os_release, UNAMELEN, "%d.%d.%d SP%d.%d",
											os_ver_info.dwMajorVersion,
											os_ver_info.dwMinorVersion,
											os_ver_info.dwBuildNumber,
											os_ver_info.wServicePackMajor,
											os_ver_info.wServicePackMinor);

	switch(sys_info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		strcpy(hi_win_mach, "amd64");
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		strcpy(hi_win_mach, "arm");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		strcpy(hi_win_mach, "ia64");
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		switch(sys_info.dwProcessorType) {
		case PROCESSOR_INTEL_386:
			strcpy(hi_win_mach, "i386");
			break;
		case PROCESSOR_INTEL_486:
			strcpy(hi_win_mach, "i486");
			break;
		case PROCESSOR_INTEL_PENTIUM:
			strcpy(hi_win_mach, "i586");
			break;
		default:
			strcpy(hi_win_mach, "x86");
			break;
		}
	}
}

PLATAPI const char* hi_get_sys_name() {
	char vendor[64] = "";
	char model[128] = "Unknown system";
	int ret;

	hi_wmi_t wmi;
	hi_wmi_iter_t iter;

	if(hi_win_sysname_found)
		return hi_win_sys_name;

	ret = hi_wmi_connect(&wmi, HI_WMI_ROOT_CIMV2);
	if(ret == HI_WMI_OK) {
		ret = hi_wmi_query(&wmi, &iter, L"WQL",
						   L"SELECT Manufacturer,Model FROM Win32_ComputerSystem");

		if(ret == HI_WMI_OK) {
			if(hi_wmi_next(&iter)) {
				hi_wmi_get_string(&iter, L"Manufacturer", vendor, 64);
				hi_wmi_get_string(&iter, L"Model", model, 128);
			}
		}

		hi_wmi_disconnect(&wmi);
	}

	snprintf(hi_win_sys_name, SYSNAMELEN, "%s %s", vendor, model);
	hi_win_sysname_found = B_TRUE;

	return hi_win_sys_name;
}

