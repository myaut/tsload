/*
 * dummy.c
 *
 *  Created on: 06.11.2012
 *      Author: myaut
 */

#define LOG_SOURCE "busy_wait"
#include <log.h>

#include <mempool.h>
#include <defs.h>
#include <workload.h>
#include <wltype.h>
#include <modules.h>
#include <modapi.h>

#include <busy_wait.h>

#include <stdlib.h>
#include <string.h>

DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("busy_wait");
DECLARE_MOD_TYPE(MOD_TSLOAD);

MODEXPORT wlp_descr_t busy_wait_params[] = {
	{ WLP_INTEGER, WLPF_NO_FLAGS,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"num_cycles",
		"Number of cycles to be waited (not CPU cycles)",
		offsetof(struct busy_wait_workload, num_cycles) },
	{ WLP_NULL }
};

module_t* self = NULL;

MODEXPORT int busy_wait_wl_config(workload_t* wl) {
	return 0;
}

MODEXPORT int busy_wait_wl_unconfig(workload_t* wl) {
	return 0;
}

MODEXPORT int busy_wait_run_request(request_t* rq) {
	struct busy_wait_workload* bww =
			(struct busy_wait_workload*) rq->rq_workload->wl_params;

	volatile wlp_integer_t i;

	for(i = 0; i < bww->num_cycles; ++i);

	return 0;
}

wl_type_t busy_wait_wlt = {
	"busy_wait",						/* wlt_name */

	WLC_CPU_MISC,						/* wlt_class */

	busy_wait_params,					/* wlt_params */
	sizeof(struct busy_wait_workload),	/* wlt_params_size*/

	busy_wait_wl_config,				/* wlt_wl_config */
	busy_wait_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,								/* wlt_wl_step */

	busy_wait_run_request				/* wlt_run_request */
};

MODEXPORT int mod_config(module_t* mod) {
	self = mod;

	wl_type_register(mod, &busy_wait_wlt);

	return MOD_OK;
}

MODEXPORT int mod_unconfig(module_t* mod) {
	wl_type_unregister(mod, &busy_wait_wlt);

	return MOD_OK;
}
