/*
 * show.c
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#include <commands.h>
#include <experiment.h>
#include <getopt.h>
#include <tstime.h>

#include <libjson.h>

#include <stdio.h>
#include <stdlib.h>

#define SHOW_NORMAL		0
#define SHOW_JSON		1
#define SHOW_LIST		2

void tse_show_json(experiment_t* exp) {
	char* json = json_write_formatted(exp->exp_config);

	if(json) {
		fputs(json, stdout);
	}

	json_free(json);
}

int tse_show_list_walk(const char* name, JSONNODE* parent, JSONNODE* node, void* context) {
	if(json_type(node) != JSON_NODE && json_type(node) != JSON_ARRAY) {
		char* value = json_as_string(node);
		printf("%s=%s\n", name, value);
		json_free(value);
	}

	return EXP_WALK_CONTINUE;
}

#define TSE_SHOW_PARAM(fmt, name, param)		\
	printf("%-12s : " fmt "\n", name, param)

#define TSE_SHOW_JSON_PARAM_STRING(agent, name)				\
	{														\
		JSONNODE* param = json_get(agent, name);			\
		if(param != NULL) {									\
			char* str = json_as_string(param);				\
			TSE_SHOW_PARAM("%s", name, str);				\
			json_free(str);									\
		}													\
	}

#define TSE_SHOW_JSON_PARAM_INT(agent, name)				\
	{														\
		JSONNODE* param = json_get(agent, name);			\
		if(param != NULL) {									\
			TSE_SHOW_PARAM("%lld", name, 					\
				(long long) json_as_int(param));			\
		}													\
	}

int tse_show_tp_walker(hm_item_t* obj, void* ctx) {
	exp_threadpool_t* etp = (exp_threadpool_t*) obj;
	char* disp = NULL;
	char quantum[40];

	if(etp->tp_disp != NULL) {
		JSONNODE_ITERATOR i_type, i_end;

		i_type = json_find(etp->tp_disp, "type");
		i_end = json_end(etp->tp_disp);

		if(i_type != i_end) {
			disp = json_as_string(*i_type);
		}
	}

	tm_human_print(etp->tp_quantum, quantum, 40);

	printf("%-16s %-6d %-8s %-12s\n", etp->tp_name,
				etp->tp_num_threads, quantum, (disp == NULL)? "???" : disp);

	if(disp != NULL) {
		json_free(disp);
	}

	return HM_WALKER_CONTINUE;
}

int tse_show_wl_walker(hm_item_t* obj, void* ctx) {
	exp_workload_t* ewl = (exp_threadpool_t*) ewl;
	char* rqsched = NULL;

	if(ewl->wl_rqsched != NULL) {
		JSONNODE_ITERATOR i_type, i_end;

		i_type = json_find(ewl->wl_rqsched, "type");
		i_end = json_end(ewl->wl_rqsched);

		if(i_type != i_end) {
			rqsched = json_as_string(*i_type);
		}
	}

	if(ewl->wl_tp_name[0] != '\0') {
		printf("%-16s %-12s %-16s %-12s\n", ewl->wl_name, ewl->wl_type,
					ewl->wl_tp_name, (rqsched == NULL)? "???" : rqsched);
	}
	else if(ewl->wl_chain_name[0] != '\0') {
		printf("%-16s %-12s %-16s %-12s chained: %s\n", ewl->wl_name, ewl->wl_type,
							"-", rqsched, ewl->wl_chain_name);
	}
	else {
		printf("%16s [ERROR]\n", ewl->wl_name);
	}

	if(rqsched != NULL) {
		json_free(rqsched);
	}

	return HM_WALKER_CONTINUE;
}

void tse_show_normal(experiment_t* exp) {
	JSONNODE* agent;
	JSONNODE* start_time;

	int err = experiment_process_config(exp);
	if(err != EXP_CONFIG_OK) {
		fprintf(stderr, "Error processing config for experiment '%s'", exp->exp_directory);
		return;
	}

	agent = experiment_cfg_find(exp->exp_config, "agent", NULL);

	TSE_SHOW_PARAM("%s", "name", exp->exp_name);
	TSE_SHOW_PARAM("%s", "root", exp->exp_root);
	TSE_SHOW_PARAM("%s", "directory", exp->exp_directory);
	TSE_SHOW_PARAM("%d", "runid", exp->exp_runid);

	if(start_time != NULL) {
		char date[32];
		tse_exp_print_start_time(exp, date, 32);
		TSE_SHOW_PARAM("%s", "start_time", date);
	}

	TSE_SHOW_PARAM("%s", "status", tse_exp_get_status_str(exp));

	if(agent != NULL) {
		TSE_SHOW_JSON_PARAM_INT(agent, "agent_pid");

		fputs("\n", stdout);

		TSE_SHOW_JSON_PARAM_STRING(agent, "hostname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "domainname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "osname");
		TSE_SHOW_JSON_PARAM_STRING(agent, "release");
		TSE_SHOW_JSON_PARAM_STRING(agent, "machine_arch");

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

