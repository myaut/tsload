
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



#ifndef RQSCHED_H_
#define RQSCHED_H_

#include <defs.h>

#include <workload.h>
#include <randgen.h>

#include <tsobj.h>

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

LIBIMPORT rqsched_class_t simple_rqsched_class;
LIBIMPORT rqsched_class_t iat_rqsched_class;
LIBIMPORT rqsched_class_t think_rqsched_class;

TESTEXPORT int tsobj_rqsched_proc(tsobj_node_t* node, workload_t* wl);

#endif /* DISP_H_ */

