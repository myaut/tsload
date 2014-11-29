
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <hostinfo/plat/wmi.h>

#include <stdio.h>
#include <assert.h>


int test_main() {
	hi_wmi_t wmi;
	hi_wmi_iter_t iter;

	char proc_name[256];

	assert(hi_wmi_connect(&wmi, HI_WMI_ROOT_CIMV2) == HI_WMI_OK);

	assert(hi_wmi_query(&wmi, &iter, L"WQL",
						L"SELECT * FROM Win32_Process") == HI_WMI_OK);

	while(hi_wmi_next(&iter)) {
		hi_wmi_get_string(&iter, L"Name", proc_name, 256);
		printf("%s\n", proc_name);
	}

	hi_wmi_disconnect(&wmi);

	return 0;
}

