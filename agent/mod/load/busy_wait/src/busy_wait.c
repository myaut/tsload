
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
	{ WLP_INTEGER, WLPF_REQUEST,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"num_cycles",
		"Number of cycles to be waited (not CPU cycles)",
		offsetof(struct busy_wait_request, num_cycles) },
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
	struct busy_wait_request* bww =
			(struct busy_wait_request*) rq->rq_params;

	volatile wlp_integer_t i;

	for(i = 0; i < bww->num_cycles; ++i);

	return 0;
}

wl_type_t busy_wait_wlt = {
	"busy_wait",						/* wlt_name */

	WLC_CPU_MISC,						/* wlt_class */

	busy_wait_params,					/* wlt_params */
	sizeof(struct busy_wait_workload),	/* wlt_params_size*/
	sizeof(struct busy_wait_request), 	/* wlt_rqparams_size*/

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

