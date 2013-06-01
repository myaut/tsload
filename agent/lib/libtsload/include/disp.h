/*
 * disp.h
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#ifndef DISP_H_
#define DISP_H_

#define DISPNAMELEN		16

typedef unsigned long long disptime_t;

/**
 * Miss policy defines dispatcher policy
 * when worker cannot execute all requests in time
 *
 * DMP_CONFIGURED - configured externally (by user)
 * DMP_DISCARD - discard requests
 * DMP_TRANSFER - execute requests during next quantum
 * DMP_ENLARGE_QUANTUM	- hold quantum until every request is executed
 * */
typedef enum {
	DMP_CONFIGURED,

	DMP_DISCARD,
	DMP_TRANSFER,
	DMP_ENLARGE_QUANTUM
} disp_miss_policy_t;

typedef struct {
	char disp_name[DISPNAMELEN];

	disptime_t (*disp_interval) ();
} disp_t;

#endif /* DISP_H_ */
