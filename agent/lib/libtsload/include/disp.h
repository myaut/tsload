/*
 * disp.h
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#ifndef DISP_H_
#define DISP_H_

#include <defs.h>

#include <workload.h>
#include <randgen.h>

#define DISPNAMELEN		16

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

#define DISP_NAME(name)		SM_INIT(.disp_name, name)

typedef struct disp_class {
	char disp_name[DISPNAMELEN];

	void (*disp_fini)(workload_t* wl);

	void (*disp_step)(workload_step_t* step);		/* Called when new step starts */

	void (*disp_pre_request)(request_t* rq);		/* Called while request is scheduling */
	void (*disp_post_request)(request_t* rq);		/* Called when request is complete */
} disp_class_t;

typedef enum {
	DD_UNIFORM,
	DD_EXPONENTIAL
} disp_distribution_t;

typedef struct disp_common {
	disp_distribution_t disp_distribution;

	randgen_t* disp_randgen;
	randvar_t* disp_randvar;

	union {
		double u_scope;
	} disp_params;

	ts_time_t last_iat;
} disp_common_t;

void disp_common_destroy(disp_common_t* disp);

#define DISP_JSON_OK			0
#define DISP_JSON_UNDEFINED		-1
#define DISP_JSON_INVALID_CLASS	-2
#define DISP_JSON_INVALID_PARAM -3

#ifndef NO_JSON
#include <libjson.h>

int json_disp_proc(JSONNODE* node, workload_t* wl);
#endif

#endif /* DISP_H_ */
