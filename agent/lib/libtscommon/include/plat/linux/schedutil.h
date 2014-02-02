/*
 * schedutil.h
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#ifndef PLAT_SCHEDUTIL_H_
#define PLAT_SCHEDUTIL_H_

#include <sched.h>

typedef struct {
	int scheduler;
	struct sched_param param;
	int nice;
} plat_sched_t;

#endif /* PLAT_SCHEDUTIL_H_ */
