
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



#include <tsload/defs.h>

#include <tsload/getopt.h>
#include <tsload/time.h>
#include <tsload/mempool.h>
#include <tsload/autostring.h>

#include <tsload/json/json.h>

#include <experiment.h>
#include <commands.h>

#include <stdio.h>
#include <string.h>


struct tse_list_item {
	int runid;

	AUTOSTRING char* name;
	char date[32];
	char hostname[64];
	char status[10];

	list_node_t node;
};

void tse_list_insert(struct tse_list_item* item, list_head_t* list);

int tse_list_walk(struct experiment_walk_ctx* ctx, void* context) {
	experiment_t* exp;
	json_node_t* j_hostname;

	struct tse_list_item* item;
	list_head_t* list = (list_head_t*) context;

	json_errno_clear();

	exp = experiment_load_dir(ctx->root->exp_root, ctx->runid, ctx->dir);

	if(exp == NULL) {
		printf("%4d [ERROR]\n", ctx->runid);
		return EXP_WALK_CONTINUE;
	}

	item = mp_malloc(sizeof(struct tse_list_item));

	list_node_init(&item->node);
	item->runid = ctx->runid;
	aas_copy(aas_init(&item->name), exp->exp_name);

	/* Get hostname */
	j_hostname = experiment_cfg_find(exp->exp_config, "agent:hostname", NULL, JSON_STRING);
	if(j_hostname != NULL) {
		strncpy(item->hostname, json_as_string(j_hostname), 64);
	}
	else {
		strcpy(item->hostname, "???");
	}

	tse_exp_print_start_time(exp, item->date, 32);
	strncpy(item->status, tse_exp_get_status_str(exp), 10);

	tse_list_insert(item, list);

	experiment_destroy(exp);

	return EXP_WALK_CONTINUE;
}

void tse_list_insert(struct tse_list_item* item, list_head_t* list) {
	struct tse_list_item* next;

	list_for_each_entry(struct tse_list_item, next, list, node) {
		if(next->runid > item->runid) {
			list_insert_back(&item->node, &next->node);
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

		aas_free(&item->name);
		mp_free(item);
	}
}

int tse_list(experiment_t* root, int argc, char* argv[]) {
	list_head_t list;

	printf("%-4s %-16s %-6s %-20s %s\n", "ID", "NAME", "STATUS", "DATE", "HOSTNAME");

	list_head_init(&list, "tse_exp_list");
	experiment_walk(root, tse_list_walk, &list, NULL);

	tse_list_print(&list);

	return CMD_OK;
}

