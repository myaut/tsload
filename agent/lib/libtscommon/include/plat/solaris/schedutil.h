/*
 * schedutil.h
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#ifndef PLAT_SCHEDUTIL_H_
#define PLAT_SCHEDUTIL_H_

#include <sys/pset.h>
#include <sys/priocntl.h>

typedef struct {
	psetid_t	pset;
	char		clname[PC_CLNMSZ];
	pc_vaparms_t pcparms;
} plat_sched_t;

#endif /* PLAT_SCHEDUTIL_H_ */
