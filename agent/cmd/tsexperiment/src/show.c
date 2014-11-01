
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#include <commands.h>
#include <experiment.h>
#include <getopt.h>
#include <tstime.h>

#include <stdio.h>
#include <stdlib.h>

#define SHOW_NORMAL		0
#define SHOW_JSON		1
#define SHOW_LIST		2

void tse_show_json(experiment_t* exp) {
	json_write_file(exp->exp_config, stdout, B_TRUE);
}

int tse_show_list_walk(const char* name, json_node_t* parent, json_node_t* node, void* context) {
	switch(json_type_hinted(node)) {
	case JSON_STRING:
		printf("%s=%s\n", name, json_as_string(node));
		break;
	case JSON_NUMBER_INTEGER:
		printf("%s=%" PRId64 "\n", name, json_as_integer(node));
		break;
	case JSON_NUMBER_FLOAT:
		printf("%s=%f\n", name, json_as_double(node));
		break;
	case JSON_BOOLEAN:
		printf("%s=%s\n", name, (json_as_boolean(node))? "true" : "false");
		break;
	}

	return EXP_WALK_CONTINUE;
}

#define TSE_SHOW_PARAM(fmt, name, param)		\
	printf("%-12s : " fmt "\n", name, param)

#define TSE_SHOW_JSON_PARAM_STRING(agent, name)								\
	{																		\
		char* str;															\
		if(json_get_string(agent, name, &str) == JSON_OK) {					\
			TSE_SHOW_PARAM("%s", name, str);								\
		}																	\
	}

#define TSE_SHOW_JSON_PARAM_INT(agent, name)								\
	{																		\
		long long ll;														\
		if(json_get_integer_ll(agent, name, &ll) == JSON_OK) {				\
			TSE_SHOW_PARAM("%lld", name, ll);								\
		}																	\
	}

int tse_show_tp_walker(hm_item_t* obj, void* ctx) {
	exp_threadpool_t* etp = (exp_threadpool_t*) obj;
	char* disp = NULL;
	char quantum[40];

	if(etp->tp_disp != NULL) {
		json_get_string(etp->tp_disp, "type", &disp);
	}

	tm_human_print(etp->tp_quantum, quantum, 40);

	printf("%-16s %-6d %-8s %-12s\n", etp->tp_name,
				etp->tp_num_threads, quantum, (disp == NULL)? "???" : disp);

	return HM_WALKER_CONTINUE;
}

int tse_show_wl_walker(hm_item_t* obj, void* ctx) {
	exp_workload_t* ewl = (exp_threadpool_t*) obj;
	char* rqsched = NULL;

	if(ewl->wl_rqsched != NULL) {
		json_get_string(ewl->wl_rqsched, "type", &rqsched);
	}

	if(ewl->wl_tp_name != NULL) {
		printf("%-16s %-12s %-16s %-12s\n", ewl->wl_name, ewl->wl_type,
					ewl->wl_tp_name, (rqsched == NULL)? "???" : rqsched);
	}
	else if(ewl->wl_chain_name != NULL) {
		printf("%-16s %-12s %-16s %-12s chained: %s\n", ewl->wl_name, ewl->wl_type,
							"-", "-", ewl->wl_chain_name);
	}
	else {
		printf("%16s [ERROR]\n", ewl->wl_name);
	}

	return HM_WALKER_CONTINUE;
}

void tse_show_normal(experiment_t* exp) {
	json_node_t* agent;
	json_node_t* start_time;

	int err = experiment_process_config(exp);
	if(err != EXP_CONFIG_OK) {
		fprintf(stderr, "Error processing config for experiment '%s': %s\n",
					exp->exp_directory, exp->exp_error_msg);
		return;
	}

	TSE_SHOW_PARAM("%s", "name", exp->exp_name);
	TSE_SHOW_PARAM("%s", "root", exp->exp_root);
	TSE_SHOW_PARAM("%s", "directory", exp->exp_directory);
	TSE_SHOW_PARAM("%d", "runid", exp->exp_runid);

	start_time = experiment_cfg_find(exp->exp_config, "start_time", NULL, JSON_NUMBER_INTEGER);
	if(start_time != NULL) {
		char date[32];
		tse_exp_print_start_time(exp, date, 32);
		TSE_SHOW_PARAM("%s", "start_time", date);
	}

	TSE_SHOW_PARAM("%s", "status", tse_exp_get_status_str(exp));

	agent = experiment_cfg_find(exp->exp_config, "agent", NULL, JSON_NODE);
	if(agent != NULL) {
		TSE_SHOW_JSON_PARAM_INT(agent, "agent_pid");

		fputs("\n", stdout);

		TSE_SHOW_JSON_PARAM_STRING(agent, "hostname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "domainname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "osname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "release");
		TSE_SHOW_JSON_PARAM_STRING(agent, "machine_arch");
		TSE_SHOW_JSON_PARAM_STRING(agent, "system");

		fputs("\n", stdout);

		TSE_SHOW_JSON_PARAM_INT(agent, "num_cpus");
		TSE_SHOW_JSON_PARAM_INT(agent, "num_cores");
		TSE_SHOW_JSON_PARAM_INT(agent, "mem_total");
	}

	printf("\n%-16s %-6s %-8s %-12s\n", "THREADPOOL", "NTHRS", "QUANTUM", "DISP");
	hash_map_walk(exp->exp_threadpools, tse_show_tp_walker, NULL);

	printf("\n%-16s %-12s %-16s %-12s\n", "WORKLOAD", "TYPE", "THREADPOOL", "RQSCHED");
	hash_map_walk(exp->exp_workloads, tse_show_wl_walker, NULL);
}

int tse_show(experiment_t* root, int argc, char* argv[]) {
	experiment_t* exp;

	int mode = SHOW_NORMAL;
	int c;

	while((c = plat_getopt(argc, argv, "jl")) != -1) {
		switch(c) {
		case 'j':
			mode = SHOW_JSON;
			break;
		case 'l':
			mode = SHOW_LIST;
			break;
		case '?':
			fprintf(stderr, "Invalid show suboption -%c\n", c);
			return CMD_INVALID_OPT;
		}
	}

	exp = tse_shift_experiment_run(root, argc, argv);
	if(exp == NULL) {
		exp = root;
	}

	switch(mode) {
	case SHOW_JSON:
		tse_show_json(exp);
		break;
	case SHOW_LIST:
		experiment_cfg_walk(exp->exp_config, tse_show_list_walk, NULL, NULL);
		break;
	case SHOW_NORMAL:
		tse_show_normal(exp);
		break;
	}

	if(exp != root) {
		experiment_destroy(exp);
	}

	return CMD_OK;
}


