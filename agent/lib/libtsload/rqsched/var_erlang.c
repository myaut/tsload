#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/time.h>

#include <tsload/load/randgen.h>
#include <tsload/load/rqsched.h>

#include <string.h>

int rqsvar_set_int_erlang(struct rqsched_var* var, const char* name, long value) {
	if(strcmp(name, "shape") == 0) {
		if(value < 1)
			return RV_INVALID_PARAM_VALUE;
		
		var->params.lval = value;
		return RV_PARAM_OK;
	}
	
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_set_double_erlang(struct rqsched_var* var, const char* name, double value) {
	return RV_INVALID_PARAM_NAME;	
}

int rqsvar_step_erlang(struct rqsched_var* var, double iat) {
	int ret;
	
	ret = rv_set_int(var->randvar, "shape", var->params.lval);
	if(ret != RV_PARAM_OK)
		return ret;
	
	return rv_set_double(var->randvar, "rate", ((double) var->params.lval) / iat);
}

randvar_param_t rqsvar_erlang_params[] = {
	{ RV_PARAM_INT, "shape", "should be 1 or more" },
	{ RV_PARAM_NULL, NULL, NULL }
};

rqsvar_class_t rqsvar_erlang_class = {
	AAS_CONST_STR("erlang"),
	&rv_erlang_class,
	rqsvar_erlang_params,
	
	SM_INIT(.rqsvar_init, NULL),
	SM_INIT(.rqsvar_destroy, NULL),
	
	SM_INIT(.rqsvar_set_int, rqsvar_set_int_erlang),
	SM_INIT(.rqsvar_set_double, rqsvar_set_double_erlang),
	
	SM_INIT(.rqsvar_step, rqsvar_step_erlang)
};