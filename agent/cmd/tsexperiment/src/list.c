/*
 * list.c
 *
 *  Created on: Feb 10, 2014
 *      Author: myaut
 */

#include <getopt.h>
#include <tstime.h>
#include <mempool.h>

#include <experiment.h>
#include <commands.h>

#include <stdio.h>
#include <string.h>

struct tse_list_item {
	int runid;

	char name[EXPNAMELEN];
	char date[32];
	char hostname[64];
	char status[8];

	list_node_t node;
};

void tse_list_insert(struct tse_list_item* item, list_head_t* list);

int tse_list_walk(struct experiment_walk_ctx* ctx, void* context) {
	experiment_t* exp;

	ts_time_t start_time;

	char* hostname = NULL;
	char* name = NULL;

	int status_code;

	JSONNODE* j_date;
	JSONNODE* j_hostname;
	JSONNODE* j_status;

	struct tse_list_item* item;
	list_head_t* list = (list_head_t*) context;

	exp = experiment_load_dir(ctx->root->exp_root, ctx->runid, ctx->dir);

	if(exp == NULL) {
		printf("%4d [ERROR]\n", ctx->runid);
		return EXP_WALK_CONTINUE;
	}

	item = mp_malloc(sizeof(struct tse_list_item));

	list_node_init(&item->node);
	item->runid = ctx->runid;
	strcpy(item->name, ctx->name);

	/* Get date */
	j_date = experiment_cfg_find(exp->exp_config, "start_time", NULL);
	if(j_date != NULL) {
		start_time = json_as_int(j_date);
		tm_datetime_print(start_time, item->date, 32);
	}
	else {
		strcpy(item->date, "???");
	}

	/* Get hostname */
	j_hostname = experiment_cfg_find(exp->exp_config, "agent:hostname", NULL);
	if(j_hostname != NULL) {
		hostname = json_as_string(j_hostname);
		strncpy(item->hostname, hostname, 64);
		json_free(hostname);
	}
	else {
		strcpy(item->hostname, "???");
	}

	j_status = experiment_cfg_find(exp->exp_config, "status", NULL);
	if(j_status != NULL) {
		status_code = json_as_int(j_status);
		switch(status_code) {
		case EXPERIMENT_OK:
			strcpy(item->status, "OK");
			break;
		case EXPERIMENT_ERROR:
			strcpy(item->status, "ERROR");
			break;
		default:
			strcpy(item->status, "UNKNOWN");
			break;
		}
	}
	else {
		strcpy(item->status, "UNKNOWN");
	}

	tse_list_insert(item, list);

	experiment_destroy(exp);

	return EXP_WALK_CONTINUE;
}

void tse_list_insert(struct tse_list_item* item, list_head_t* list) {
	struct tse_list_item* next;

	list_for_each_entry(struct tse_list_item, next, list, node) {
		if(next->runid > item->runid) {
			list_insert_before(&item->node, &next->node);
			return;
		}
	}

	list_add_tail(&item->node, list);
}

void tse_list_print(list_head_t* list) {
	struct tse_list_item* item;
	struct tse_list_item* next;

	list_for_each_entry_safe(struct tse_list_item, item, next, list, node) {
		printf("%-4d %-16s %-6s %-20s %s\n", item->runid, item->name,
				item->status, item->date, item->hostname);
	}
}

int tse_list(experiment_t* root, int argc, char* argv[]) {
	list_head_t list;

	printf("%-4s %-16s %-6s %-20s %s\n", "ID", "NAME", "STATUS", "DATE", "HOSTNAME");

	list_head_init(&list, "tse_exp_list");
	experiment_walk(root, tse_list_walk, &list);

	tse_list_print(&list);

	return CMD_OK;
}
