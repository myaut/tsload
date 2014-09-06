/*
 * wlpgen.c
 *
 *  Created on: Jan 25, 2014
 *      Author: myaut
 */

#define NO_JSON
#define JSONNODE void

#include <wlparam.h>
#include <workload.h>
#include <mempool.h>
#include <list.h>
#include <randgen.h>
#include <tsload.h>
#include <errcode.h>
#include <field.h>

#include <tsobj.h>

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

int tsobj_wlpgen_proc_pmap(tsobj_node_t* node, wlp_generator_t* gen);
void wlpgen_destroy_pmap(wlp_generator_t* gen, int pcount, wlpgen_probability_t* pmap);

static wlp_generator_t* wlpgen_create(wlpgen_type_t type, wlp_descr_t* wlp, workload_t* wl) {
	wlp_generator_t* gen = (wlp_generator_t*) mp_malloc(sizeof(wlp_generator_t));

	gen->type = type;
	gen->wlp = wlp;
	gen->wl = wl;

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

	return wlparam_set_default(wlp, value->value, wl);
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
int tsobj_wlpgen_param_proc(tsobj_node_t* node, wlp_generator_t* gen, wlpgen_value_t* value) {
	if(wlp_get_base_type(gen->wlp) == WLP_RAW_STRING) {
		value->string = mp_malloc(gen->wlp->range.str_length);
		return tsobj_wlparam_proc(node, gen->wlp, value->string, gen->wl);
	}

	return tsobj_wlparam_proc(node, gen->wlp, value->value, gen->wl);
}

int tsobj_wlpgen_proc(tsobj_node_t* node, wlp_descr_t* wlp, struct workload* wl) {
	wlp_generator_t* gen;
	int ret = WLPARAM_TSOBJ_OK;
	int err1, err2;

	randgen_t* rg = NULL;
	randvar_t* rv = NULL;

	tsobj_node_t *o_randgen, *o_randvar, *o_pmap;

	/* Parsing configuration */
	if(tsobj_type(node) != JSON_NODE) {
		gen = wlpgen_create(WLPG_VALUE, wlp, wl);
		ret = tsobj_wlpgen_param_proc(node, gen, &gen->generator.value);
		goto end;
	}

	if(tsobj_get_node(node, "randgen", &o_randgen) != TSOBJ_OK)
		goto bad_tsobj;

	/* Now pick pmap and randvar with following limitations (as the follow in code):
	 * 	- pmap should be an array
	 * 	- non-number parameters support only pmap
	 * 	- randvar should be a node (and optional since uniform distribution may be handled by randgen)
	 * 	- pmap and randvar are mutually exclusive */
	err2 = tsobj_get_array(node, "pmap", &o_pmap);
	if(err2 == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(err2 != TSOBJ_OK &&
			(wlp_get_base_type(wlp) != WLP_INTEGER &&
			 wlp_get_base_type(wlp) != WLP_FLOAT)) {
		goto bad_tsobj;
	}

	err1 = tsobj_get_node(node, "randvar", &o_randvar);
	if(err1 == TSOBJ_INVALID_TYPE)
		goto bad_tsobj;

	if(err2 == TSOBJ_OK && err1 == TSOBJ_OK) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_ERROR_PREFIX "'randvar' and 'pmap' are mutually exclusive",
						 wl->wl_name, wlp->name);
		return WLPARAM_TSOBJ_BAD;
	}

	if(tsobj_check_unused(node) != TSOBJ_OK)
		goto bad_tsobj;

	rg = tsobj_randgen_proc(o_randgen);
	if(rg == NULL) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_ERROR_PREFIX "failed to create random generator",
						 wl->wl_name, wlp->name);
		return WLPARAM_ERROR;
	}

	if(err1 == TSOBJ_OK) {
		rv = tsobj_randvar_proc(o_randvar, rg);

		if(rv == NULL) {
			rg_destroy(rg);
			tsload_error_msg(TSE_INVALID_VALUE,
							 WLP_ERROR_PREFIX "failed to create random variator",
							 wl->wl_name, wlp->name);
			return WLPARAM_ERROR;
		}
	}

	/*  Finally create a generator */
	gen = wlpgen_create_random(wlp, wl, rg, rv);

	if(err2 == TSOBJ_OK) {
		ret = tsobj_wlpgen_proc_pmap(o_pmap, gen);
	}

end:
	if(ret != WLPARAM_TSOBJ_OK) {
		wlpgen_destroy(gen);
	}

	return ret;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), WLP_ERROR_PREFIX "%s",
					 wl->wl_name, wlp->name, tsobj_error_message());

	return WLPARAM_TSOBJ_BAD;
}

int tsobj_wlpgen_proc_parray(tsobj_node_t* node, wlp_generator_t* gen, wlpgen_probability_t* pmap, int pid) {
	int ret = WLPARAM_ERROR;
	int id;

	tsobj_node_t* el;

	if(tsobj_size(node) == 0) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_PMAP_ERROR_PREFIX " 'valarray' is empty",
						 gen->wl->wl_name, gen->wlp->name, pid, gen->wlp->name);
		return WLPARAM_TSOBJ_BAD;
	}

	pmap->length = tsobj_size(node);
	pmap->valarray = mp_malloc(sizeof(wlpgen_value_t) * pmap->length);

	tsobj_for_each(node, el, id)  {
		ret = tsobj_wlpgen_param_proc(el, gen, &pmap->valarray[id]);

		if(ret != WLPARAM_TSOBJ_OK) {
			/* Save number of correctly parsed array elements so destroy()
			 * may handle that situation */
			pmap->length = id + 1;
			return ret;
		}
	}

	return ret;
}

int tsobj_wlpgen_proc_pmap(tsobj_node_t* node, wlp_generator_t* gen) {
	wlpgen_randgen_t* randgen = &gen->generator.randgen;
	int pid = 0, pcount;

	wlpgen_probability_t* pmap;

	double total = 0.0, probability, diff, eps;
	int ret;
	int err1, err2;

	tsobj_node_t* el;
	tsobj_node_t *value, *valarray;

	if(tsobj_size(node) == 0) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_ERROR_PREFIX " 'pmap' is empty",
						 gen->wl->wl_name, gen->wlp->name, gen->wlp->name);
		return WLPARAM_TSOBJ_BAD;
	}

	pcount = tsobj_size(node);
	pmap = mp_malloc(pcount * sizeof(wlpgen_probability_t));

	tsobj_for_each(node, el, pid) {
		pmap[pid].length = 0;
		pmap[pid].valarray = NULL;

		if(tsobj_check_type(el, JSON_NODE) != TSOBJ_OK)
			goto bad_tsobj;

		if(tsobj_get_double(el, "probability", &probability) != TSOBJ_OK)
			goto bad_tsobj;

		total += probability;

		/* There are two levels of probability map values. Basically, pmap is O(n), because
		 * there is cycle that used in wlpgen_gen_pmap() Second level (array) allows to pick
		 * value with equal probabilities for O(1) complexity times and represented by valarray.  */
		value = tsobj_find(el, "value");
		valarray = tsobj_find_opt(el, "valarray");

		if(value == NULL && valarray == NULL)
			goto bad_tsobj;

		if(tsobj_check_unused(el) != TSOBJ_OK)
			goto bad_tsobj;

		if(tsobj_check_type(valarray, JSON_ARRAY) == TSOBJ_INVALID_TYPE)
			goto bad_tsobj;

		if(value != NULL && valarray != NULL) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 WLP_PMAP_ERROR_PREFIX "neither 'value' nor 'valarray' was defined",
							 gen->wl->wl_name, gen->wlp->name, pid);
			goto error;
		}

		if(value != NULL && valarray != NULL) {
			tsload_error_msg(TSE_INVALID_VALUE,
							 WLP_PMAP_ERROR_PREFIX "'value' and 'valarray' are mutually exclusive",
							 gen->wl->wl_name, gen->wlp->name, pid);
			goto error;
		}

		if(value != NULL) {
			ret = tsobj_wlpgen_param_proc(value, gen, &pmap[pid].value);
		}
		else if(valarray != NULL) {
			ret = tsobj_wlpgen_proc_parray(valarray, gen, &pmap[pid], pid);
		}

		if(ret != WLPARAM_TSOBJ_OK) {
			goto error;
		}

		pmap[pid].probability = probability;
	}

	/* We may incur some errors during probability calculations,
	 * so we are being nice with small errors.
	 * XXX: Is calculation of eps is correct?*/
	diff = total - 1.0;
	if((diff > wlpgen_pmap_eps) || (diff < -wlpgen_pmap_eps)) {
		tsload_error_msg(TSE_INVALID_VALUE,
						 WLP_PMAP_ERROR_PREFIX " invalid total probability %f",
						 gen->wl->wl_name, gen->wlp->name, -1, gen->wlp->name, total);
		ret = WLPARAM_ERROR;
		/* For most "errors" in foreach loop we want to do pid + 1 (to rollback all elements including current,
		 * but in this case pid is already points to the last element, so we need to adjust that.
		 * (Yes, this is a hack) */
		--pid;
		goto error;
	}

	/* Everything went fine */
	randgen->pcount = pcount;
	randgen->pmap = pmap;

	return ret;

bad_tsobj:
	tsload_error_msg(tsobj_error_code(), WLP_PMAP_ERROR_PREFIX "%s",
					 gen->wl->wl_name, gen->wlp->name, pid, tsobj_error_message());
	ret = WLPARAM_TSOBJ_BAD;
error:
	wlpgen_destroy_pmap(gen, pid + 1, pmap);
	return ret;
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
