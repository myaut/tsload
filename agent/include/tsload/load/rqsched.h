
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

#include <tsload/defs.h>

#include <tsload/autostring.h>

#include <tsload/obj/obj.h>

#include <tsload/load/workload.h>
#include <tsload/load/randgen.h>


/**
 * @module Request schedulers
 */

#define RQSVARHASHSIZE			8
#define RQSVARHASHMASK			((RQSVARHASHSIZE) - 1)

struct rqsched;
struct rqsched_var;
struct tsload_param;

#define RQSCHED_NO_FLAGS		0x00
#define RQSCHED_NEED_VARIATOR	0x01

#define RQSVAR_RANDGEN_PARAM	{ TSLOAD_PARAM_RANDGEN, "randgen", "optional" }

typedef struct rqsvar_class {
	AUTOSTRING char* rqsvar_name;
	
	randvar_class_t* rqsvar_rvclass;
	struct tsload_param* rqsvar_params;
	
	int (*rqsvar_init)(struct rqsched_var* var);
	void (*rqsvar_destroy)(struct rqsched_var* var);
	
	int (*rqsvar_set_int)(struct rqsched_var* var, const char* name, long value);
	int (*rqsvar_set_double)(struct rqsched_var* var, const char* name, double value);
	
	int (*rqsvar_step)(struct rqsched_var* var, double iat);
	
	module_t* rqsvar_module;
	struct rqsvar_class* rqsvar_next;
} rqsvar_class_t;

#define RQSCHED_NAME(name)		SM_INIT(.rqsched_name, AAS_CONST_STR(name))

typedef struct rqsched_class {
	AUTOSTRING char* rqsched_name;
	const char* rqsched_description;
	
	int rqsched_flags;
	struct tsload_param* rqsched_params;

	int  (*rqsched_proc_tsobj)(tsobj_node_t* node, workload_t* wl, struct rqsched* rqs);
	void (*rqsched_fini)(workload_t* wl, struct rqsched* rqs);

	void (*rqsched_step)(workload_step_t* step);		/* Called when new step starts */

	void (*rqsched_pre_request)(request_t* rq);		/* Called while request is scheduling */
	void (*rqsched_post_request)(request_t* rq);		/* Called when request is complete */
	
	module_t* rqsched_module;
	struct rqsched_class* rqsched_next;
} rqsched_class_t;

typedef struct rqsched_var {
	rqsvar_class_t* class;
	randgen_t* randgen;
	randvar_t* randvar;
	
	union {
		double dval;
		long   lval;
		void*  private;
	} params;
} rqsched_var_t;

typedef struct rqsched {
	rqsched_class_t* rqs_class;
	rqsched_var_t* rqs_var;
	void* rqs_private;
} rqsched_t;

STATIC_INLINE void rqsvar_step(rqsched_t* rqs, double iat) {	
	rqs->rqs_var->class->rqsvar_step(rqs->rqs_var, iat);
}

#define RQSCHED_TSOBJ_OK			0
#define RQSCHED_TSOBJ_ERROR			-1
#define RQSCHED_TSOBJ_BAD			-2
#define RQSCHED_TSOBJ_RG_ERROR		-3

rqsched_t* rqsched_create(rqsched_class_t* rqs_class);
TESTEXPORT void rqsched_destroy(workload_t* wl);

tsobj_node_t* tsobj_rqsvar_class_format(rqsvar_class_t* var_class);
tsobj_node_t* tsobj_rqsched_class_format(rqsched_class_t* rqs_class);

int tsobj_rqsched_proc_randgen(tsobj_node_t* node, const char* param, randgen_t** p_randgen);
TESTEXPORT int tsobj_rqsched_proc(tsobj_node_t* node, workload_t* wl);

LIBEXPORT int rqsvar_register(module_t* mod, rqsvar_class_t* rqsvar_class);
LIBEXPORT int rqsvar_unregister(module_t* mod, rqsvar_class_t* rqsvar_class);

LIBEXPORT rqsched_class_t* rqsched_find(const char* class_name);
LIBEXPORT int rqsched_register(module_t* mod, rqsched_class_t* rqs_class);
LIBEXPORT int rqsched_unregister(module_t* mod, rqsched_class_t* rqs_class);

LIBEXPORT int rqsched_init(void);
LIBEXPORT void rqsched_fini(void);

#endif /* DISP_H_ */

