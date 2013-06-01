/*
 * simple.c
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <disp.h>

disptime_t simple_disp_interval(void) {
	return 0;
}

disp_t simple_disp = {
	SM_INIT(.disp_name, "simple"),
	SM_INIT(.disp_interval, simple_disp_interval)
};
