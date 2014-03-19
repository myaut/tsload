/*
 * wmi_proc.c
 *
 *  Created on: Mar 19, 2014
 *      Author: myaut
 */

#include <plat/win/wmi.h>

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
