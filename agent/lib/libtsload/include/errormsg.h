/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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


#ifndef ERRORMSG_H_
#define ERRORMSG_H_

#define RV_ERROR_PREFIX 			"Failed to create random variator: "
#define RQSCHED_ERROR_PREFIX		"Failed to create request scheduler for workload '%s': "

#define TP_ERROR_SCHED_PREFIX		"Failed to schedule threadpool '%s': "
#define TPD_ERROR_PREFIX			"Failed to parse dispatcher: "

#define WLP_ERROR_PREFIX 			"Failed to set workload '%s' parameter '%s': "
#define WLP_PMAP_ERROR_PREFIX		WLP_ERROR_PREFIX "pmap element #%d: "
#define WL_ERROR_PREFIX				"Failed to create workload '%s': "
#define WL_CHAIN_ERROR_PREFIX		"Failed to chain workload '%s': "

#define TSLOAD_CONFIGURE_WORKLOAD_ERROR_PREFIX		"Couldn't configure workload '%s': "
#define TSLOAD_PROVIDE_STEP_ERROR_PREFIX 			"Couldn't provide step for workload '%s': "
#define TSLOAD_CREATE_REQUEST_ERROR_PREFIX 			"Couldn't provide request for workload '%s'"
#define TSLOAD_START_WORKLOAD_ERROR_PREFIX			"Couldn't start workload '%s': "
#define TSLOAD_UNCONFIGURE_WORKLOAD_ERROR_PREFIX	"Couldn't unconfigure workload '%s': "

#define TSLOAD_CREATE_THREADPOOL_ERROR_PREFIX		"Couldn't create threadpool '%s': "
#define TSLOAD_SCHED_THREADPOOL_ERROR_PREFIX		"Couldn't schedule threadpool '%s': "
#define TSLOAD_DESTROY_THREADPOOL_ERROR_PREFIX		"Couldn't destroy threadpool '%s': "

extern tsload_error_msg_func tsload_error_msg;

#endif /* ERRORMSG_H_ */
