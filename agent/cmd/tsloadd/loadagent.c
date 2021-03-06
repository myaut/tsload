
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



#define LOG_SOURCE "agent"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/load/workload.h>
#include <tsload.h>

#include <tsload/agent/client.h>
#include <tsload/agent/agent.h>

#include <hostinfo/uname.h>
#include <hostinfo/cpuinfo.h>

#include <loadagent.h>

#include <string.h>


/* = Agent's handlers = */
void agent_get_host_info(JSONNODE* argv[]) {
	JSONNODE* response = tsload_get_hostinfo();

	return agent_response_msg(response);
}

void agent_get_workload_types(JSONNODE* argv[]) {
	JSONNODE* response = tsload_walk_workload_types(TSLOAD_WALK_JSON_ALL, NULL, NULL);

	return agent_response_msg(response);
}

void agent_get_resources(JSONNODE* argv[]) {
	JSONNODE* response = tsload_get_resources();

	return agent_response_msg(response);
}


void agent_configure_workload(JSONNODE* argv[]) {
	char* wl_name = json_as_string(argv[0]);
	JSONNODE* wl_params = (JSONNODE*) argv[1];

	int ret;

	ret = tsload_configure_workload(wl_name, wl_params);

	json_free(wl_name);

	/* Send response if no error encountered */
	if(ret == TSLOAD_OK) {
		agent_response_msg(json_new(JSON_NODE));
	}
}

void agent_start_workload(JSONNODE* argv[]) {
	/* While ts_time_t may hold time until 26th century, json_as_int
	 * provides signed values, so it will work until Sat Apr 12 03:47:16 2262 */
	ts_time_t start_time = (ts_time_t) json_as_int(argv[1]);
	char* wl_name = json_as_string(argv[0]);

	int ret;

	ret = tsload_start_workload(wl_name, start_time);

	json_free(wl_name);

	/* Send response if no error encountered */
	if(ret == TSLOAD_OK) {
		agent_response_msg(json_new(JSON_NODE));
	}
}

void agent_provide_step(JSONNODE* argv[]) {
	int ret = 0;
	int status;

	char* wl_name = json_as_string(argv[0]);
	long step_id = json_as_int(argv[1]);
	unsigned num_rqs = json_as_int(argv[2]);

	JSONNODE* response = NULL;

	/* Call tsload function */
	ret = tsload_provide_step(wl_name, step_id, num_rqs, &status);

	json_free(wl_name);

	/* Send response if no error encountered */
	if(ret == TSLOAD_OK) {
		response = json_new(JSON_NODE);
		json_push_back(response, json_new_b("success", status == WL_STEP_OK));
		agent_response_msg(response);
	}
}

void agent_create_threadpool(JSONNODE* argv[]) {
	char* tp_name = json_as_string(argv[0]);
	unsigned num_threads = json_as_int(argv[1]);
	ts_time_t quantum = json_as_int(argv[2]);
	boolean_t discard = json_as_bool(argv[3]);
	JSONNODE* disp = argv[4];

	int ret = 0;

	ret = tsload_create_threadpool(tp_name, num_threads, quantum,
								   discard, disp);

	json_free(tp_name);

	if(ret == TSLOAD_OK) {
		agent_response_msg(json_new(JSON_NODE));
	}
}

void agent_get_threadpools(JSONNODE* argv[]) {
	JSONNODE* response = json_new(JSON_NODE);

	JSONNODE* tp_info = tsload_get_threadpools();
	JSONNODE* disp_info = tsload_get_dispatchers();

	json_set_name(tp_info, "threadpools");
	json_push_back(response, tp_info);

	json_set_name(disp_info, "dispatchers");
	json_push_back(response, disp_info);

	return agent_response_msg(response);
}

void agent_destroy_threadpool(JSONNODE* argv[]) {
	char* tp_name = json_as_string(argv[0]);

	int ret = 0;

	ret = tsload_destroy_threadpool(tp_name);

	if(ret == TSLOAD_OK) {
		agent_response_msg(json_new(JSON_NODE));
	}
}

/* = TSServer's methods = */
void agent_workload_status(const char* wl_name, int status, long progress, const char* config_msg) {
	JSONNODE* message = json_new(JSON_NODE);
	JSONNODE* response;

	json_push_back(message, json_new_a("workload_name", wl_name));
	json_push_back(message, json_new_i("status", status));
	json_push_back(message, json_new_i("progress", progress));
	json_push_back(message, json_new_a("message", config_msg));

	clnt_invoke("workload_status", message, &response);

	json_delete(response);
}

void agent_requests_report(list_head_t* rq_list) {
	JSONNODE *msg = json_new(JSON_NODE), *arg0 = json_new(JSON_NODE);
	JSONNODE *response;

	JSONNODE *j_rq_list = json_request_format_all(rq_list);

	json_set_name(arg0, "requests");
	json_push_back(msg, arg0);

	json_set_name(j_rq_list, "requests");
	json_push_back(arg0, j_rq_list);

	clnt_invoke("requests_report", msg, &response);

	json_delete(response);
}

static agent_dispatch_t loadagent_table[] = {
	AGENT_METHOD("getHostInfo",
		ADT_ARGS(),
		agent_get_host_info),
	AGENT_METHOD("getWorkloadTypes",
		ADT_ARGS(),
		agent_get_workload_types),
	AGENT_METHOD("getResources",
			ADT_ARGS(),
			agent_get_resources),
	AGENT_METHOD("configure_workload",
		ADT_ARGS(
			ADT_ARGUMENT("workload_name", JSON_STRING),
			ADT_ARGUMENT("workload_params", JSON_NODE),
		),
		agent_configure_workload),
	AGENT_METHOD("start_workload",
		ADT_ARGS(
			ADT_ARGUMENT("workload_name", JSON_STRING),
			ADT_ARGUMENT("start_time", JSON_NUMBER),
		),
		agent_start_workload),
	AGENT_METHOD("provide_step",
		ADT_ARGS(
			ADT_ARGUMENT("workload_name", JSON_STRING),
			ADT_ARGUMENT("step_id", JSON_NUMBER),
			ADT_ARGUMENT("num_requests", JSON_NUMBER),
		),
		agent_provide_step),
	AGENT_METHOD("create_threadpool",
		ADT_ARGS(
			ADT_ARGUMENT("tp_name", JSON_STRING),
			ADT_ARGUMENT("num_threads", JSON_NUMBER),
			ADT_ARGUMENT("quantum", JSON_NUMBER),
			ADT_ARGUMENT("discard", JSON_BOOL),
			ADT_ARGUMENT("disp", JSON_NODE),
		),
		agent_create_threadpool),
	AGENT_METHOD("get_threadpools",
		ADT_ARGS(),
		agent_get_threadpools),
	AGENT_METHOD("destroy_threadpool",
		ADT_ARGS(
			ADT_ARGUMENT("tp_name", JSON_STRING),
		),
		agent_destroy_threadpool),

	ADT_LAST_METHOD()
};

int agent_init(void) {
	agent_register_methods(loadagent_table);

	strncpy(agent_type, "load", AGENTTYPELEN);

	tsload_error_msg = agent_error_msg;
	tsload_workload_status = agent_workload_status;
	tsload_requests_report = agent_requests_report;

	return clnt_init();
}

void agent_fini(void) {
	clnt_fini();
}

