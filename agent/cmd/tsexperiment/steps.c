
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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

#include <tsload/hashmap.h>
#include <tsload/mempool.h>

#include <tsload/load/workload.h>
#include <tsload.h>
#include <tsload/posixdecl.h>

#include <steps.h>
#include <tseerror.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* TODO: Implement TSFile steps */

int step_get_step_file(steps_file_t* sf, long* step_id, unsigned* p_num_rqs);
int step_get_step_const(steps_const_t* sc, long* step_id, unsigned* p_num_rqs);
int step_get_step_trace(steps_trace_t* st, long* p_step_id, unsigned* p_num_rqs, list_head_t* rq_list);

typedef struct step_workload_trace {
	list_head_t requests;

	exp_workload_t* ewl;
	list_node_t node;
} step_workload_trace_t;

typedef struct step_request_trace {
	list_node_t node;
	struct step_request_trace* chain_next;

	exp_request_entry_t* rqe;
	void* rq_params;
} step_request_trace_t;

step_request_trace_t* step_trace_create_rq(exp_workload_t* ewl);
int step_trace_fetch_step(experiment_t* exp, step_workload_trace_t* stwl, long step_id, unsigned num_rqs);
void step_trace_insert_rq(step_request_trace_t* strq, step_workload_trace_t* stwl);
void step_trace_destroy_rqs(step_workload_trace_t* stwl);
unsigned step_trace_count_chained_rqs(step_workload_trace_t* stwl);

steps_generator_t* step_create_generator(steps_generator_type_t type) {
	steps_generator_t* sg;

	sg = mp_malloc(sizeof(steps_generator_t));
	sg->sg_type = type;

	return sg;
}

steps_generator_t* step_create_file(const char* file_name, const char* out_file_name) {
	steps_generator_t* sg;
	steps_file_t* sf;
	FILE* file = fopen(file_name, "r");
	FILE* fileout;
	struct stat statbuf;

	if(!file)
		return NULL;

	if(stat(out_file_name, &statbuf) == 0) {
		fclose(file);
		return NULL;
	}

	fileout = fopen(out_file_name, "w");

	if(!fileout) {
		fclose(file);
		return NULL;
	}

	sg = step_create_generator(STEPS_FILE);

	sf = &sg->sg_file;
	sf->sf_file = file;
	sf->sf_file_out = fileout;
	sf->sf_step_id = 0;
	sf->sf_error = B_FALSE;

	return sg;
}

steps_generator_t* step_create_const(long num_steps, unsigned num_requests) {
	steps_generator_t* sg;

	if(num_steps < 0)
		return NULL;

	sg = step_create_generator(STEPS_CONST);

	sg->sg_const.sc_step_id = 0;
	sg->sg_const.sc_num_steps = num_steps;
	sg->sg_const.sc_num_requests = num_requests;

	return sg;
}

steps_generator_t* step_create_trace(steps_generator_t* parent, experiment_t* base, exp_workload_t* ewl) {
	steps_generator_t* sg;
	step_workload_trace_t* swt;

	exp_workload_t* base_ewl;

	sg = step_create_generator(STEPS_TRACE);

	sg->sg_trace.st_parent = parent;
	sg->sg_trace.st_exp = base;
	sg->sg_trace.st_ewl = ewl;

	list_head_init(&sg->sg_trace.st_wl_chain, "st-wl-%s", ewl->wl_name);

	do {
		base_ewl = hash_map_find(base->exp_workloads, ewl->wl_name);

		if(base_ewl == NULL) {
			step_destroy(sg);
			return NULL;
		}

		swt = mp_malloc(sizeof(step_workload_trace_t));

		swt->ewl = base_ewl;
		list_head_init(&swt->requests, "swt-%s", ewl->wl_name);
		list_node_init(&swt->node);

		list_add_tail(&swt->node, &sg->sg_trace.st_wl_chain);

		ewl = ewl->wl_chain_next;
	} while(ewl != NULL);

	return sg;
}

void step_destroy_trace(steps_generator_t* sg) {
	step_workload_trace_t* swt;
	step_workload_trace_t* swt_next;

	list_for_each_entry_safe(step_workload_trace_t, swt, swt_next,
			                 &sg->sg_trace.st_wl_chain, node) {
		assert(list_empty(&swt->requests));

		list_del(&swt->node);
		mp_free(swt);
	}
}

static long step_parse_line(char* line) {
	/* Check if all characters in string are digits*/
	char* p = line;

	while(*p) {
		if(isspace(*p)) {
			*p = '\0';
			break;
		}

		if(!isdigit(*p))
			return STEP_ERROR;

		++p;
	}

	/* Read and process number of requests
	 * No need for checking of negative values,
	 * because we filter minus sign earlier*/
	return atol(line);
}

void step_close_file(steps_file_t* sf) {
	fclose(sf->sf_file);
	fclose(sf->sf_file_out);
}

int step_get_step(steps_generator_t* sg, long* step_id, unsigned* p_num_rqs, list_head_t* trace_rqs) {
	switch(sg->sg_type) {
	case STEPS_FILE:
		return step_get_step_file(&sg->sg_file, step_id, p_num_rqs);
	case STEPS_CONST:
		return step_get_step_const(&sg->sg_const, step_id, p_num_rqs);
	case STEPS_TRACE:
		return step_get_step_trace(&sg->sg_trace, step_id, p_num_rqs, trace_rqs);
	}

	return STEP_ERROR;
}

int step_get_step_file(steps_file_t* sf, long* step_id, unsigned* p_num_rqs) {
	char step_str[16];
	char* p;
	long num_rqs;

	*step_id = sf->sf_step_id;

	if(sf->sf_error) {
		return STEP_ERROR;
	}

	/* Read next step from file */
	fgets(step_str, 16, sf->sf_file);

	/* There are no more steps on file */
	if(feof(sf->sf_file) != 0) {
		return STEP_NO_RQS;
	}

	num_rqs = step_parse_line(step_str);

	if(num_rqs < 0) {
		sf->sf_error = B_TRUE;
		return STEP_ERROR;
	}

	*p_num_rqs = num_rqs;

	sf->sf_step_id++;

	fprintf(sf->sf_file_out, "%ld\n", num_rqs);

	return STEP_OK;
}

int step_get_step_const(steps_const_t* sc, long* p_step_id, unsigned* p_num_rqs) {
	if(sc->sc_step_id == sc->sc_num_steps) {
		return STEP_NO_RQS;
	}

	*p_step_id = sc->sc_step_id;
	*p_num_rqs = sc->sc_num_requests;

	sc->sc_step_id++;

	return STEP_OK;
}

int step_get_step_trace(steps_trace_t* st, long* p_step_id, unsigned* p_num_rqs, list_head_t* rq_list) {
	long step_id;
	unsigned num_requests;
	unsigned num_rqs_chained;

	step_workload_trace_t* stwl;
	step_workload_trace_t* stwl_chain;

	step_request_trace_t* strq;
	step_request_trace_t* strq_chain;

	int ret = STEP_OK;
	int err;

	ret = step_get_step(st->st_parent, &step_id, &num_requests, rq_list);
	if(ret != STEP_OK)
		return ret;

	/* Fetch */
	num_rqs_chained = num_requests;
	list_for_each_entry(step_workload_trace_t, stwl,
						&st->st_wl_chain, node) {
		ret = step_trace_fetch_step(st->st_exp, stwl, step_id, num_rqs_chained);

		if(ret != STEP_OK) {
			goto end;
		}

		num_rqs_chained = step_trace_count_chained_rqs(stwl);
	}

	/* Bind chained requests by their ids */
	list_for_each_entry_safe(step_workload_trace_t, stwl, stwl_chain,
							 &st->st_wl_chain, node) {
		if(!list_is_last(&stwl->node, &st->st_wl_chain)) {
			strq_chain = list_first_entry(step_request_trace_t, &stwl_chain->requests, node);

			if(list_empty(&stwl->requests)) {
				continue;
			}

			list_for_each_entry(step_request_trace_t, strq, &stwl->requests, node) {
				if(&strq_chain->node == list_head_node(&stwl_chain->requests)) {
					break;
				}

				if(strq->rqe->rq_request != strq_chain->rqe->rq_request) {
					/* Root and chain requests should have identical ids */
					continue;
				}

				strq->chain_next = strq_chain;
				strq_chain = list_next_entry(step_request_trace_t, strq_chain, node);
			}
		}
	}

	/* Fill rq_list */
	stwl = list_first_entry(step_workload_trace_t, &st->st_wl_chain, node);
	list_for_each_entry(step_request_trace_t, strq, &stwl->requests, node) {
		strq_chain = strq->chain_next;
		stwl_chain = list_next_entry(step_workload_trace_t, stwl, node);

		err = tsload_create_request(stwl->ewl->wl_name, rq_list, B_FALSE,
									strq->rqe->rq_request, step_id, strq->rqe->rq_user, strq->rqe->rq_thread,
									strq->rqe->rq_sched_time, strq->rq_params);

		if(err != TSLOAD_OK) {
			tse_experiment_error_msg(st->st_exp, EXPERR_STEPS_TRACE_REQUEST_ERROR,
							"Failed to create trace request #%d for workload '%s'\n",
							strq->rqe->rq_request, st->st_ewl->wl_name);
			ret = STEP_ERROR;
			goto end;
		}

		while(strq_chain != NULL) {
			err = tsload_create_request(stwl_chain->ewl->wl_name, rq_list, B_TRUE,
					strq_chain->rqe->rq_request, step_id, strq_chain->rqe->rq_user, strq_chain->rqe->rq_thread,
					strq_chain->rqe->rq_sched_time, strq_chain->rq_params);

			if(err != TSLOAD_OK) {
				tse_experiment_error_msg(st->st_exp, EXPERR_STEPS_TRACE_CHAINED_ERROR,
						"Failed to create chained trace request #%d for root workload '%s'\n",
						strq->rqe->rq_request, st->st_ewl->wl_name);
				ret = STEP_ERROR;
				goto end;
			}

			strq_chain = strq_chain->chain_next;
			stwl_chain = list_next_entry(step_workload_trace_t, stwl_chain, node);
		}
	}

	*p_step_id = step_id;
	*p_num_rqs = num_requests;

end:
	/* Destroy */
	list_for_each_entry(step_workload_trace_t, stwl,
	 					&st->st_wl_chain, node) {
		step_trace_destroy_rqs(stwl);
	}

	return ret;
}

step_request_trace_t* step_trace_create_rq(exp_workload_t* ewl) {
	step_request_trace_t* strq;
	size_t rqe_size;

	rqe_size = ewl->wl_file_schema->hdr.entry_size;

	strq = mp_malloc(rqe_size + sizeof(step_request_trace_t));

	list_node_init(&strq->node);
	strq->chain_next = NULL;

	strq->rqe = ((char*) strq) + sizeof(step_request_trace_t);
	strq->rq_params = ((char*) strq->rqe) + sizeof(exp_request_entry_t);

	return strq;
}

int step_trace_fetch_step(experiment_t* exp, step_workload_trace_t* stwl, long step_id, unsigned num_rqs) {
	step_request_trace_t* strq;
	exp_workload_t* ewl = stwl->ewl;
	int next_index = -1;
	int index = ewl->wl_file_index;

	unsigned fetched_rqs = 0;
	int err;

	uint32_t rq_count = tsfile_get_count(ewl->wl_file);

	/* Fetch requests for step. Because order of requests inside this file is undetermined
	 * we save index of beginning of next step and scan entire file until all num_rqs are
	 * loaded (so if .tss file would be altered, trace would fail). For example we need to
	 * load 5 requests of step 3:
	 *
	 * (2, 5) (3, 0) (2, 6) (3, 1) (3, 3) (3, 2) (3, 3) (4, 0) (3, 4) (4, 1)
	 *        ^index                                    ^next_index   |
	 *                                                                + exit here
	 * tuples are (rq_step, rq_id)
	 *  */

	while(fetched_rqs < num_rqs) {
		if(index == rq_count) {
			/* Request count provided by upper generator differs
			 * from number of reported requests. Trace failed :( */
			tse_experiment_error_msg(exp, EXPERR_STEPS_END_OF_TRACE,
									 "Workload '%s': TSFile ended prematurely on %d entry\n", 
									  stwl->ewl->wl_name, index);
			return STEP_ERROR;
		}

		strq = step_trace_create_rq(ewl);

		while(index < rq_count) {
			err = tsfile_get_entries(ewl->wl_file, strq->rqe, index, index + 1);
			if(err != TSFILE_OK) {
				tse_experiment_error_msg(exp, EXPERR_STEPS_TRACE_TSFILE_ERROR,
										 "Workload '%s': TSFile read error: %d\n", 
							             stwl->ewl->wl_name, err);

				mp_free(strq);
				return STEP_ERROR;
			}

			if(strq->rqe->rq_step == step_id) {
				/* This is request of current step - link it */
				step_trace_insert_rq(strq, stwl);
				++index;
				++fetched_rqs;
				break;
			}
			else if((strq->rqe->rq_step > step_id) && next_index == -1) {
				/* Found beginning of next step */
				next_index = index;
			}

			++index;
		}
	}

	if(next_index == -1)
		next_index = index;

	ewl->wl_file_index = next_index;

	return STEP_OK;
}

void step_trace_insert_rq(step_request_trace_t* strq, step_workload_trace_t* stwl) {
	step_request_trace_t* prev;

	/* Insert request sorted by their id's (useful for binding chaining requests) */

	if(!list_empty(&stwl->requests)) {
		prev = list_last_entry(step_request_trace_t, &stwl->requests, node);

		if(prev->rqe->rq_request < strq->rqe->rq_request) {
			/* Fastpath */
			list_add_tail(&strq->node, &stwl->requests);
			return;
		}

		list_for_each_entry_continue_reverse(step_request_trace_t, prev, &stwl->requests, node) {
			if(prev->rqe->rq_request < strq->rqe->rq_request) {
				list_insert_front(&strq->node, &prev->node);
				return;
			}
		}
	}

	list_add(&strq->node, &stwl->requests);
}

void step_trace_destroy_rqs(step_workload_trace_t* stwl) {
	step_request_trace_t* strq;
	step_request_trace_t* strq_next;

	list_for_each_entry_safe(step_request_trace_t, strq, strq_next, &stwl->requests, node) {
		list_del(&strq->node);
		mp_free(strq);
	}
}

unsigned step_trace_count_chained_rqs(step_workload_trace_t* stwl) {
	step_request_trace_t* strq;
	unsigned count = 0;

	list_for_each_entry(step_request_trace_t, strq, &stwl->requests, node) {
		if(strq->rqe->rq_chain_request >= 0) {
			++count;
		}
	}

	return count;
}

void step_destroy(steps_generator_t* sg) {
	if(sg->sg_type == STEPS_FILE)
		step_close_file(&sg->sg_file);
	if(sg->sg_type == STEPS_TRACE)
		step_destroy_trace(sg);

	mp_free(sg);
}


