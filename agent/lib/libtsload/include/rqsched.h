/*
 * rqsched.h
 *
 *  Created on: 04.12.2012
 *      Author: myaut
 */

#ifndef RQSCHED_H_
#define RQSCHED_H_

#include <defs.h>

#include <workload.h>
#include <randgen.h>

#define RQSCHEDNAMELEN		16

#define RQSCHED_NAME(name)		SM_INIT(.rqsched_name, name)

/**
 * @module Request schedulers
 */

typedef struct rqsched_class {
	char rqsched_name[RQSCHEDNAMELEN];

	void (*rqsched_fini)(workload_t* wl);

	void (*rqsched_step)(workload_step_t* step);		/* Called when new step starts */

	void (*rqsched_pre_request)(request_t* rq);		/* Called while request is scheduling */
	void (*rqsched_post_request)(request_t* rq);		/* Called when request is complete */
} rqsched_class_t;

typedef enum {
	RQSD_UNIFORM,
	RQSD_EXPONENTIAL,
	RQSD_ERLANG,
	RQSD_NORMAL
} rqsched_distribution_t;

typedef struct rqsched_common {
	rqsched_distribution_t rqs_distribution;

	randgen_t* rqs_randgen;
	randvar_t* rqs_randvar;

	union {
		double u_scope;
		int    e_shape;
		double n_dispersion;
	} rqs_params;
} rqsched_common_t;

typedef struct rqsched_think {
	rqsched_common_t common;

	randgen_t* rqs_user_randgen;
	int rqs_nusers;
} rqsched_think_t;

void rqsched_common_destroy(rqsched_common_t* disp);

#define RQSCHED_TSOBJ_OK			0
#define RQSCHED_TSOBJ_ERROR			-1
#define RQSCHED_TSOBJ_BAD			-2
#define RQSCHED_TSOBJ_RG_ERROR		-3

#define RQSCHED_ERROR_PREFIX		"Failed to create request scheduler for workload '%s': "

#ifndef NO_JSON
#include <libjson.h>
STATIC_INLINE int json_rqsched_proc(JSONNODE* node, workload_t* wl) { return 0; }
#define RQSCHED_JSON_OK				0
#else
#include <tsobj.h>
int tsobj_rqsched_proc(tsobj_node_t* node, workload_t* wl);
#endif

#endif /* DISP_H_ */
