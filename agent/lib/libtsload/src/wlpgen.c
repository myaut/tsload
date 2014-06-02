
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



#include <wlparam.h>
#include <workload.h>
#include <mempool.h>
#include <list.h>
#include <randgen.h>
#include <tsload.h>
#include <errcode.h>
#include <field.h>

#include <math.h>
#include <float.h>
#include <assert.h>

/**
 * #### Workload parameter generators
 *
 * Some of workload parameters may be set on per-request basis. For example,
 * disk i/o generator with random or sequental access to blocks needs number
 * of block per each request. Thus, it would be generated in this module.
 *
 * There are two main generators:
 *  * __WLPG_VALUE__ - uses same value would be used for all requests. If
 *    value is omitted in config, _wlpgen_ will try to use default value.
 *  * __WLPG_RANDOM__ - like it's name says, it will generate value per each
 *    request using random generators and variators.
 *      * for integer and float wlparam types it will take pure generator/variator value
 *      * for other wlparam types it uses probability map and random generator only
 */

DECLARE_FIELD_FUNCTIONS(wlp_integer_t);
DECLARE_FIELD_FUNCTIONS(wlp_float_t);
DECLARE_FIELD_FUNCTIONS(wlp_bool_t);
DECLARE_FIELD_FUNCTIONS(wlp_strset_t);
DECLARE_FIELD_FUNCTIONS(wlp_hiobject_t);

double wlpgen_pmap_eps = 0.00001;

int json_wlpgen_proc_pmap(JSONNODE* node, wlp_generator_t* gen);
void wlpgen_destroy_pmap(wlp_generator_t* gen, int pcount, wlpgen_probability_t* pmap);

static wlp_generator_t* wlpgen_create(wlpgen_type_t type, wlp_descr_t* wlp, workload_t* wl) {
	wlp_generator_t* gen = (wlp_generator_t*) mp_malloc(sizeof(wlp_generator_t));

	gen->type = type;
	gen->wlp = wlp;

	list_add_tail(&gen->node, &wl->wl_wlpgen_head);

	return gen;
}

int wlpgen_create_default(wlp_descr_t* wlp, struct workload* wl) {
	wlp_generator_t* gen = wlpgen_create(WLPG_VALUE, wlp, wl);
	wlpgen_value_t* value = &gen->generator.value;

	if(wlp_get_base_type(wlp) == WLP_RAW_STRING) {
		value->string = mp_malloc(strlen(wlp->defval.s) + 1);
		strcpy(value->string, wlp->defval.s);

		return WLPARAM_DEFAULT_OK;
	}

	return wlparam_set_default(wlp, value->value);
}

wlp_generator_t* wlpgen_create_random(wlp_descr_t* wlp, struct workload* wl,
									  randgen_t* rg, randvar_t* rv) {
	wlp_generator_t* gen = wlpgen_create(WLPG_RANDOM, wlp, wl);
	wlpgen_randgen_t* randgen = &gen->generator.randgen;

	randgen->rg = rg;
	randgen->rv = rv;
	randgen->pcount = 0;
	randgen->pmap = NULL;

	return gen;
}

void wlpgen_destroy(wlp_generator_t* gen) {
	if(gen->type == WLPG_RANDOM) {
		wlpgen_randgen_t* randgen = &gen->generator.randgen;

		wlpgen_destroy_pmap(gen, randgen->pcount, randgen->pmap);

		rg_destroy(randgen->rg);
		if(randgen->rv != NULL) {
			rv_destroy(randgen->rv);
		}
	}
	else {
		wlpgen_value_t* value = &gen->generator.value;

		if(wlp_get_base_type(gen->wlp) == WLP_RAW_STRING) {
			mp_free(value->string);
		}
	}

	list_del(&gen->node);
	mp_free(gen);
}

void wlpgen_destroy_all(struct workload* wl) {
	wlp_generator_t* gen;
	wlp_generator_t* gen_next;

	list_for_each_entry_safe(wlp_generator_t, gen, gen_next,
							 &wl->wl_wlpgen_head, node) {
		wlpgen_destroy(gen);
	}
}

void wlpgen_destroy_pmap(wlp_generator_t* gen, int pcount, wlpgen_probability_t* pmap) {
	int pid;
	int vi;

	wlpgen_probability_t* probability;

	if(wlp_get_base_type(gen->wlp) == WLP_RAW_STRING) {
		for(pid = 0; pid < pcount; ++pid) {
			probability = pmap + pid;

			if(probability->length != 0) {
				for(vi = 0; vi < probability->length; ++vi) {
					mp_free(probability->valarray[vi].string);
				}
			}
			else {
				mp_free(probability->value.string);
			}
		}
	}

	for(pid = 0; pid < pcount; ++pid) {
		probability = pmap + pid;
		if(probability->length > 0) {
			mp_free(probability->valarray);
		}
	}

	if(pmap != NULL) {
		mp_free(pmap);
	}
}

/* Because wlpgen are dynamically generated, we couldn't know in compile-time
 * length of wlparam-string, so they are dynamically allocated and handled
 * specially in this helper function */
int json_wlpgen_param_proc(JSONNODE* node, wlp_generator_t* gen, wlpgen_value_t* value) {
	if(wlp_get_base_type(gen->wlp) == WLP_RAW_STRING) {
		value->string = mp_malloc(gen->wlp->range.str_length);
		return json_wlparam_string_proc(node, gen->wlp, value->string);
	}

	return json_wlparam_proc(node, gen->wlp, value->value);
}

int json_wlpgen_proc(JSONNODE* node, wlp_descr_t* wlp, struct workload* wl) {
	wlp_generator_t* gen;
	int ret = WLPARAM_JSON_OK;

	randgen_t* rg = NULL;
	randvar_t* rv = NULL;
	JSONNODE_ITERATOR i_end, i_randgen, i_randvar, i_pmap;

	if(json_type(node) != JSON_NODE) {
		gen = wlpgen_create(WLPG_VALUE, wlp, wl);
		return json_wlpgen_param_proc(node, gen, &gen->generator.value);
	}

	/* Random generated value */

	i_end = json_end(node);
	i_randgen = json_find(node, "randgen");
	i_randvar = json_find(node, "randvar");
	i_pmap = json_find(node, "pmap");

	if(i_pmap == i_end) {
		if(wlp_get_base_type(wlp) != WLP_INTEGER && wlp_get_base_type(wlp) != WLP_FLOAT) {
			tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX ": missing probability map", wlp->name);
			return WLPARAM_MISSING_PMAP;
		}
	}

	if(i_randgen == i_end) {
		tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX ": missing 'randgen'", wlp->name);
		return WLPARAM_MISSING_RANDSPEC;
	}

	rg = json_randgen_proc(*i_randgen);

	if(rg == NULL) {
		tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX ": failed to parse random generator", wlp->name);
		return WLPARAM_MISSING_RANDSPEC;
	}

	if(i_randvar != i_end) {
		rv = json_randvar_proc(*i_randvar, rg);

		if(rv == NULL) {
			rg_destroy(rg);
			tsload_error_msg(TSE_INVALID_DATA, WLP_ERROR_PREFIX ": failed to parse random variator", wlp->name);
			return WLPARAM_MISSING_RANDSPEC;
		}
	}

	gen = wlpgen_create_random(wlp, wl, rg, rv);

	if(i_pmap != i_end) {
		ret = json_wlpgen_proc_pmap(*i_pmap, gen);
	}

	return ret;
}

int json_wlpgen_proc_parray(JSONNODE* node, wlp_generator_t* gen, wlpgen_probability_t* pmap) {
	JSONNODE_ITERATOR i_el, i_end;
	int ret = WLPARAM_INVALID_PMAP;
	int vi = 0;

	if(json_size(node) == 0) {
		return WLPARAM_INVALID_PMAP;
	}

	i_end = json_end(node);
	i_el = json_begin(node);

	pmap->length = json_size(node);
	pmap->valarray = mp_malloc(sizeof(wlpgen_value_t) * pmap->length);

	while(i_el != i_end) {
		ret = json_wlpgen_param_proc(*i_el, gen, &pmap->valarray[vi]);

		if(ret != WLPARAM_JSON_OK)
			return ret;

		++vi;
		++i_el;
	}

	return ret;
}

int json_wlpgen_proc_pmap(JSONNODE* node, wlp_generator_t* gen) {
	wlpgen_randgen_t* randgen = &gen->generator.randgen;
	int pid = 0, pcount;
	wlpgen_probability_t* pmap;
	double total = 0.0, probability, diff, eps;
	int ret;

	JSONNODE_ITERATOR i_el, i_end;
	JSONNODE_ITERATOR i_value, i_valarray, i_probability, i_el_end;

	if(json_type(node) != JSON_ARRAY) {
		tsload_error_msg(TSE_MESSAGE_FORMAT,
						 WLP_ERROR_PREFIX ": probability map is not an array", gen->wlp->name);
		return WLPARAM_INVALID_PMAP;
	}

	pcount = json_size(node);
	pmap = mp_malloc(pcount * sizeof(wlpgen_probability_t));

	i_end = json_end(node);
	i_el = json_begin(node);

	while(i_el != i_end) {
		if(json_type(*i_el) != JSON_NODE) {
			tsload_error_msg(TSE_MESSAGE_FORMAT,
							 WLP_ERROR_PREFIX ": pmap element #%d should be a node",
							 gen->wlp->name, pid);
			wlpgen_destroy_pmap(gen, pid, pmap);
			return WLPARAM_INVALID_PMAP;
		}

		i_el_end = json_end(*i_el);
		i_probability = json_find(*i_el, "probability");
		i_value = json_find(*i_el, "value");
		i_valarray = json_find(*i_el, "valarray");

		if(i_probability == i_el_end  ||
				json_type(*i_probability) != JSON_NUMBER) {
			tsload_error_msg(TSE_MESSAGE_FORMAT,
							 WLP_ERROR_PREFIX ": pmap element #%d - missing probability",
							 gen->wlp->name, pid);
			wlpgen_destroy_pmap(gen, pid, pmap);
			return WLPARAM_INVALID_PMAP;
		}

		probability = json_as_float(*i_probability);
		total += probability;

		/* There are two levels of probability map values. Basically, pmap is O(n), because
		 * there is cycle that used in wlpgen_gen_pmap() Second level (array) allows to pick
		 * value with equal probabilities for O(1) complexity times and represented by valarray.  */
		if(i_value != i_el_end) {
			pmap[pid].length = 0;
			pmap[pid].valarray = NULL;
			ret = json_wlpgen_param_proc(*i_value, gen, &pmap[pid].value);
		}
		else if(i_valarray != i_el_end && json_type(*i_valarray) == JSON_ARRAY) {
			ret = json_wlpgen_proc_parray(*i_valarray, gen, &pmap[pid]);
		}
		else {
			tsload_error_msg(TSE_MESSAGE_FORMAT,
							 WLP_ERROR_PREFIX ": pmap element #%d - neither 'value' nor 'valarray' was defined",
							 gen->wlp->name, pid);
			wlpgen_destroy_pmap(gen, pid, pmap);
			return WLPARAM_INVALID_PMAP;
		}

		if(ret != WLPARAM_JSON_OK) {
			tsload_error_msg(TSE_INVALID_DATA,
							 WLP_ERROR_PREFIX ": pmap element #%d - failed to parse value",
							 gen->wlp->name, pid);
			wlpgen_destroy_pmap(gen, pid, pmap);
			return ret;
		}

		pmap[pid].probability = probability;

		++i_el;
		++pid;
	}

	/* We may incur some errors during probability calculations,
	 * so we are being nice with small errors.
	 * XXX: Is calculation of eps is correct?*/
	diff = total - 1.0;
	if((diff > wlpgen_pmap_eps) || (diff < -wlpgen_pmap_eps)) {
		tsload_error_msg(TSE_INVALID_DATA,
						 WLP_ERROR_PREFIX ": probability map - invalid total probability %f",
						 gen->wlp->name, total);
		wlpgen_destroy_pmap(gen, pid, pmap);
		return WLPARAM_INVALID_PMAP;
	}

	/* Everything went fine */
	randgen->pcount = pcount;
	randgen->pmap = pmap;

	return WLPARAM_JSON_OK;
}

#define WLPGEN_GEN_VALUE(type, value, param)				\
	FIELD_PUT_VALUE(type, param, * (type*) &value->value)
#define WLPGEN_GEN_RANDOM(type, value, param)				\
	FIELD_PUT_VALUE(type, param, value)

void wlpgen_gen_value(wlp_generator_t* gen, wlpgen_value_t* value, void* param) {
	switch(wlp_get_base_type(gen->wlp)) {
	case WLP_BOOL:
		WLPGEN_GEN_VALUE(wlp_bool_t, value, param);
		break;
	case WLP_INTEGER:
		WLPGEN_GEN_VALUE(wlp_integer_t, value, param);
		break;
	case WLP_FLOAT:
		WLPGEN_GEN_VALUE(wlp_float_t, value, param);
		break;
	case WLP_RAW_STRING:
		strncpy((char*) param, value->string,
				gen->wlp->range.str_length);
		break;
	case WLP_STRING_SET:
		WLPGEN_GEN_VALUE(wlp_strset_t, value, param);
		break;
	case WLP_HI_OBJECT:
		WLPGEN_GEN_VALUE(wlp_hiobject_t, value, param);
		break;
	}
}

void wlpgen_gen_pmap_value(wlp_generator_t* gen, double val, wlpgen_probability_t* probability, void* param) {
	int vi;

	if(probability->length == 0) {
		wlpgen_gen_value(gen, &probability->value, param);
	}
	else {
		/* Normalize value to [0.0; 1.0] then pick element from array */
		val /= probability->probability;
		vi = (int) (val * probability->length);

		assert(vi < probability->length);

		wlpgen_gen_value(gen, &probability->valarray[vi], param);
	}
}

void wlpgen_gen_pmap(wlp_generator_t* gen, wlpgen_randgen_t* randgen, void* param) {
	double val = rg_generate_double(randgen->rg);
	int pid;

	wlpgen_probability_t* probability;

	/* `val` is in [0.0; 1.0)
	 * If val is below than current probability then pick pmap element
	 * Otherwise, reduce val by current probability then try next element
	 *
	 * Assuming that S is sum of probabilities for elements 1..n-1,
	 * p is probability for current element and v is generated value,
	 * element n would be picked if v < (S + p) and v >= S
	 *
	 * We do not keep S, but substruct it from v each time: v' < p  */
	for(pid = 0; pid < randgen->pcount; ++pid) {
		probability = &randgen->pmap[pid];

		if(val < probability->probability) {
			wlpgen_gen_pmap_value(gen, val, probability, param);
			return;
		}

		val -= probability->probability;
	}

	/* no match - maybe small calculations error -> use last pmap element */
	probability = &randgen->pmap[randgen->pcount - 1];
	wlpgen_gen_pmap_value(gen, val, probability, param);
}

void wlpgen_gen_random(wlp_generator_t* gen, void* param) {
	wlpgen_randgen_t* randgen = &gen->generator.randgen;
	double val;
	uint64_t ival;

	if(randgen->pcount == 0) {
		if(randgen->rv == NULL) {
			switch(wlp_get_base_type(gen->wlp)) {
			case WLP_INTEGER:
				ival = rg_generate_int(randgen->rg);
				WLPGEN_GEN_RANDOM(wlp_integer_t, ival, param);
				break;
			case WLP_FLOAT:
				val = rg_generate_double(randgen->rg);
				WLPGEN_GEN_RANDOM(wlp_float_t, val, param);
				break;
			}
		}
		else {
			switch(gen->wlp->type) {
			case WLP_INTEGER:
				val = rv_variate_double(randgen->rv);
				WLPGEN_GEN_RANDOM(wlp_integer_t, round(val), param);
				break;
			case WLP_FLOAT:
				val = rv_variate_double(randgen->rv);
				WLPGEN_GEN_RANDOM(wlp_float_t, val, param);
				break;
			}
		}

		return;
	}

	wlpgen_gen_pmap(gen, randgen, param);
}

/**
 * Generate request parameter structure for workload wl
 *
 * @param wl workload
 *
 * @return pointer to that structure or NULL if no request params \
 *         exist for this workload type
 *
 * @note structure is freed on wl_request_destroy()
 */
void* wlpgen_generate(struct workload* wl) {
	wlp_generator_t* gen;
	char* rq_params;
	char* param;

	int ret;

	if(wl->wl_type->wlt_rqparams_size == 0) {
		return NULL;
	}

	rq_params = mp_malloc(wl->wl_type->wlt_rqparams_size);

	list_for_each_entry(wlp_generator_t, gen, &wl->wl_wlpgen_head, node) {
		param = ((char*) rq_params) + gen->wlp->off;

		if(gen->type == WLPG_VALUE) {
			wlpgen_gen_value(gen, &gen->generator.value, param);
		}
		else {
			wlpgen_gen_random(gen, param);
		}
	}

	return rq_params;
}

