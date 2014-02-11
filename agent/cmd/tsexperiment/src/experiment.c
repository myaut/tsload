/*
 * experiment.c
 *
 *  Created on: Feb 8, 2014
 *      Author: myaut
 */

#define LOG_SOURCE "experiment"
#include <log.h>

#include <mempool.h>
#include <tsdirent.h>
#include <pathutil.h>
#include <hashmap.h>
#include <tsfile.h>
#include <plat/posixdecl.h>
#include <wlparam.h>
#include <wltype.h>
#include <tsload.h>
#include <tstime.h>

#include <experiment.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

static char* strrchr_y(const char* s, int c) {
	/* TODO: fallback to libc if possible. */
	char* from = s + strlen(s);

    do {
        if(from == s)
            return NULL;

        --from;
    } while(*from != c);

    return from;
}

DECLARE_HASH_MAP_STRKEY(exp_workload_hash_map, exp_workload_t, EWLHASHSIZE, wl_name, wl_next, EWLHASHMASK);
DECLARE_HASH_MAP_STRKEY(exp_tp_hash_map, exp_threadpool_t, ETPHASHSIZE, tp_name, tp_next, ETPHASHMASK);

tsfile_schema_t request_schema = {
	TSFILE_SCHEMA_HEADER(sizeof(exp_request_entry_t), 10),
	{
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_step, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_request, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_chain_request, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_thread, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_user, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_sched_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_start_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_end_time, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_queue_length, TSFILE_FIELD_INT),
		TSFILE_SCHEMA_FIELD_REF(exp_request_entry_t, rq_flags, TSFILE_FIELD_INT),
	}
};

/**
 * Reads JSON config of experiment from file and parses it
 *
 * @return LOAD_OK if everything was OK or LOAD_ERR_ constant
 */
static int experiment_read_config(experiment_t* exp) {
	int ret = EXP_LOAD_OK;
	char* data;
	size_t length;
	FILE* file;

	char experiment_filename[PATHMAXLEN];

	path_join(experiment_filename, PATHMAXLEN,
			  exp->exp_root, exp->exp_directory, EXPERIMENT_FILENAME, NULL);

	file = fopen(experiment_filename, "r");

	if(!file) {
		logmsg(LOG_CRIT, "Couldn't open workload file %s", experiment_filename);
		return EXP_LOAD_ERR_OPEN_FAIL;
	}

	/* Get length of file than rewind */
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* Read data from file */
	data = mp_malloc(length + 1);
	fread(data, length, 1, file);
	data[length] = '\0';

	/* Parse JSON */
	exp->exp_config = json_parse(data);

	mp_free(data);
	fclose(file);

	if(exp->exp_config == NULL) {
		logmsg(LOG_CRIT, "JSON parse error of file %s", experiment_filename);
		ret = EXP_LOAD_ERR_BAD_JSON;
	}

	return ret;
}

static void experiment_init(experiment_t* exp, const char* root_path, int runid, const char* dir) {
	strcpy(exp->exp_root, root_path);
	strncpy(exp->exp_directory, dir, PATHPARTMAXLEN);

	exp->exp_name[0] = '\0';
	exp->exp_basedir[0] = '\0';

	exp->exp_runid = runid;

	exp->exp_config = NULL;
	exp->exp_threadpools = NULL;
	exp->exp_workloads = NULL;

	mutex_init(&exp->exp_mutex, "exp-%s", exp->exp_directory);
	cv_init(&exp->exp_cv, "exp-%s", exp->exp_directory);

	exp->exp_wl_count = 0;
	exp->exp_wl_running_count = 0;
	exp->exp_wl_configuring_count = 0;

	exp->exp_log = NULL;

	exp->exp_error = 0;
	exp->exp_status = EXPERIMENT_NOT_CONFIGURED;

	exp->exp_trace_mode = B_FALSE;
}

static exp_threadpool_t* exp_tp_create(const char* name) {
	exp_threadpool_t* etp = mp_malloc(sizeof(exp_threadpool_t));

	strncpy(etp->tp_name, name, TPNAMELEN);

	etp->tp_discard = B_FALSE;
	etp->tp_quantum = 0;
	etp->tp_disp = NULL;
	etp->tp_sched = NULL;

	etp->tp_next = NULL;

	etp->tp_status = EXPERIMENT_NOT_CONFIGURED;

	return etp;
}

int exp_tp_destroy_walker(hm_item_t* obj, void* ctx) {
	mp_free(obj);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

static exp_workload_t* exp_wl_create(const char* name) {
	 exp_workload_t* ewl = mp_malloc(sizeof(exp_workload_t));

	 strncpy(ewl->wl_name, name, WLNAMELEN);

	 ewl->wl_type[0] = '\0';
	 ewl->wl_tp_name[0] = '\0';

	 ewl->wl_deadline = TS_TIME_MAX;

	 ewl->wl_rqsched = NULL;
	 ewl->wl_params = NULL;
	 ewl->wl_chain = NULL;

	 ewl->wl_chain_next = NULL;
	 ewl->wl_next = NULL;

	 ewl->wl_chain_name[0] = '\0';

	 ewl->wl_steps = NULL;
	 ewl->wl_steps_cfg = NULL;

	 ewl->wl_is_chained = B_FALSE;

	 ewl->wl_file_schema = NULL;
	 ewl->wl_file = NULL;
	 ewl->wl_file_flags = EXP_OPEN_NONE;
	 ewl->wl_file_index = 0;

	 ewl->wl_status = EXPERIMENT_UNKNOWN;

	 return ewl;
}

int exp_wl_destroy_walker(hm_item_t* obj, void* ctx) {
	exp_workload_t* ewl = (exp_workload_t*) obj;

	if(ewl->wl_file != NULL) {
		tsfile_close(ewl->wl_file);
	}

	if(ewl->wl_file_schema != NULL) {
		mp_free(ewl->wl_file_schema);
	}

	mp_free(ewl);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}


/**
 * Loads config of experiment run by it's directory
 *
 * @param root_path root path of experiment
 * @param runid id of run (not used while loading)
 * @param dir subdirectory in root_path that contains run
 *
 * To load experiment by it's runid, use experiment_load_run()
 *
 * @return experiment or NULL if experiment was failed to load
 */
experiment_t* experiment_load_dir(const char* root_path, int runid, const char* dir) {
	experiment_t* exp = mp_malloc(sizeof(experiment_t));
	JSONNODE* status;
	JSONNODE* name;
	char* name_str;

	experiment_init(exp, root_path, runid, dir);

	if(experiment_read_config(exp) != EXP_LOAD_OK) {
		mp_free(exp);
		return NULL;
	}

	/* Fetch status & name if possibles */
	status = experiment_cfg_find(exp->exp_config, "status", NULL);
	if(status != NULL) {
		exp->exp_status = json_as_int(status);
	}

	name = experiment_cfg_find(exp->exp_config, "name", NULL);
	if(name != NULL) {
		name_str = json_as_string(name);
		strncpy(exp->exp_name, name_str, EXPNAMELEN);
		json_free(name_str);
	}

	return exp;
}

/**
 * Load current config of experiment
 *
 * @param path root path of experiment
 *
 * @return experiment or NULL if experiment was failed to load
 */
experiment_t* experiment_load_root(const char* path) {
	return experiment_load_dir(path, EXPERIMENT_ROOT, "");
}

static int experiment_load_run_walk(struct experiment_walk_ctx* ctx, void* context) {
	int* p_runid = (int*) context;

	if(*p_runid == ctx->runid)
		return EXP_WALK_RETURN;

	return EXP_WALK_CONTINUE;
}


/**
 * Load config of experiment run by it's runid
 *
 * @param root current experiment config
 * @param runid id of experiment's run
 *
 * @return experiment or NULL if experiment was failed to load or runid doesn't exist
 */
experiment_t* experiment_load_run(experiment_t* root, int runid) {
	return experiment_walk(root, experiment_load_run_walk, &runid);
}

static int experiment_max_runid_walk(struct experiment_walk_ctx* ctx, void* context) {
	long* p_runid = (long*) context;

	if(ctx->runid > *p_runid) {
		*p_runid = (long) ctx->runid;
	}

	return EXP_WALK_CONTINUE;
}

/**
 * Generating experiment id. It is cached in file experiment.id.
 * If cache-file found, simply read value from that. If no such file
 * exists, walk over entire experiment directory and find latest
 * experiment id. Rewrite "experiment.id" in any case.
 *
 * @return -1 on error or next experiment id
 */
static long experiment_generate_id(experiment_t* root) {
	FILE* runid_file;
	long runid = -1;

	char runid_path[PATHMAXLEN];
	char runid_str[32];

	path_join(runid_path, PATHMAXLEN, root->exp_root, EXPID_FILENAME, NULL);

	runid_file = fopen(runid_path, "r");
	if(runid_file != NULL) {
		/* FIXME: Locking on Windows*/
		flock(fileno(runid_file), LOCK_EX);

		fgets(runid_str, 32, runid_file);
		runid = strtol(runid_str, NULL, 10);
	}

	if(runid == -1) {
		experiment_walk(root, experiment_max_runid_walk, &runid);
	}
	++runid;

	if(runid_file != NULL) {
		runid_file = freopen(runid_path, "w", runid_file);
	}
	else {
		runid_file = fopen(runid_path, "w");

		if(runid_file == NULL) {
			return -1;
		}

		flock(fileno(runid_file), LOCK_EX);
	}
	fprintf(runid_file, "%ld\n", runid);
	fclose(runid_file);

	return runid;
}

/**
 * Create new experiment run. Allocates new run id and
 * clones config of experiment exp.
 *
 * @param root current experiment config
 * @param base experiment config to be cloned. May be set to NULL, though \
 * 			root config would be used
 * @param name custom name of experiment
 */
experiment_t* experiment_create(experiment_t* root, experiment_t* base, const char* name) {
	long runid = experiment_generate_id(root);
	experiment_t* exp = NULL;
	char dir[PATHPARTMAXLEN];

	char* cfg_name = NULL;
	JSONNODE* j_name;

	if(runid == -1) {
		logmsg(LOG_CRIT, "Couldn't allocate runid for experiment!");
		return NULL;
	}

	if(base == NULL)
		base = root;

	if(name == NULL) {
		j_name = json_get(base->exp_config, "name");
		if(j_name == NULL) {
			logmsg(LOG_CRIT, "Neither command-line name nor name from config was provided");
			return NULL;
		}

		name = cfg_name = json_as_string(j_name);
	}

	snprintf(dir, PATHPARTMAXLEN, "%s-%ld", name, runid);

	exp = mp_malloc(sizeof(experiment_t));
	experiment_init(exp, root->exp_root, runid, dir);

	strcpy(exp->exp_basedir, base->exp_directory);
	strcpy(exp->exp_name, name);

	exp->exp_config = json_copy(base->exp_config);

	experiment_cfg_add(exp->exp_config, NULL, json_new_a("name", name), B_TRUE);

	if(cfg_name != NULL) {
		json_free(cfg_name);
	}

	return exp;
}

/**
 * Destroy experiment object
 */
void experiment_destroy(experiment_t* exp) {
	if(exp->exp_threadpools != NULL) {
		hash_map_walk(exp->exp_threadpools, exp_tp_destroy_walker, NULL);
		hash_map_destroy(exp->exp_threadpools);
	}

	if(exp->exp_workloads != NULL) {
		hash_map_walk(exp->exp_workloads, exp_wl_destroy_walker, NULL);
		hash_map_destroy(exp->exp_workloads);
	}

	if(exp->exp_config != NULL) {
		json_delete(exp->exp_config);
	}

	if(exp->exp_log != NULL) {
		fclose(exp->exp_log);
	}

	cv_destroy(&exp->exp_cv);
	mutex_destroy(&exp->exp_mutex);

	mp_free(exp);
}

/**
 * Tries to create experiment subdirectory
 */
int experiment_mkdir(experiment_t* exp) {
	char path[PATHMAXLEN];
	struct stat statbuf;

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		return EXPERIMENT_MKDIR_INVALID;
	}

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);

	if(stat(path, &statbuf) == 0) {
		return EXPERIMENT_MKDIR_EXISTS;
	}

	if(mkdir(path, S_IRWXU |
				   S_IRGRP | S_IXGRP |
				   S_IROTH | S_IXOTH) != 0) {
		return EXPERIMENT_MKDIR_ERROR;
	}

	return EXPERIMENT_MKDIR_OK;
}

/**
 * Write experiment to file experiment.json in it's subdir
 */
int experiment_write(experiment_t* exp) {
	char path[PATHMAXLEN];
	char old_path[PATHMAXLEN];

	FILE* file;
	char* data;

	struct stat statbuf;

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_FILENAME, NULL);
	path_join(old_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_FILENAME_OLD, NULL);

	if(stat(path, &statbuf) == 0) {
		if(rename(path, old_path) == -1) {
			logmsg(LOG_WARN, "Failed to rename experiment's config '%s' -> '%s'", path, old_path);
			return EXPERIMENT_WRITE_RENAME_ERROR;
		}
	}

	file = fopen(path, "w");
	if(file == NULL) {
		logmsg(LOG_WARN, "Failed to open experiment's config '%s'", path);

		rename(old_path, path);
		return EXPERIMENT_WRITE_FILE_ERROR;
	}

	data = json_write_formatted(exp->exp_config);
	fputs(data, file);
	json_free(data);

	fclose(file);

	return EXPERIMENT_WRITE_OK;
}

/**
 * Opens experiment log
 */
int experiment_open_log(experiment_t* exp) {
	char path[PATHMAXLEN];
	struct stat statbuf;

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		return EXPERIMENT_OPENLOG_INVALID;
	}

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_LOG_FILE, NULL);

	if(stat(path, &statbuf) == 0) {
		return EXPERIMENT_OPENLOG_EXISTS;
	}

	exp->exp_log = fopen(path, "w");

	if(exp->exp_log == NULL) {
		return EXPERIMENT_OPENLOG_ERROR;
	}

	return EXPERIMENT_MKDIR_OK;
}

/**
 * Walk over experiment runs and call pred() for each run
 *
 * pred() should return one of EXP_WALK_* values
 *
 * @note if run contains subruns inside it, experiment_walk() will ignore them
 */
experiment_t* experiment_walk(experiment_t* root, experiment_walk_func pred, void* context) {
	plat_dir_t* dir = plat_opendir(root->exp_root);
	plat_dir_entry_t* de = NULL;

	experiment_t* exp = NULL;

	char name[PATHPARTMAXLEN];
	char* ptr;
	int ret;

	struct experiment_walk_ctx ctx;

	ctx.root = root;

	while((de = plat_readdir(dir)) != NULL) {
		if(plat_dirent_hidden(de) || plat_dirent_type(de) != DET_DIR)
			continue;

		strncpy(name, de->d_name, PATHPARTMAXLEN);

		ptr = strrchr_y(name, '-');
		if(ptr == NULL)
			continue;

		*ptr++ = '\0';

		ctx.runid = strtol(ptr, NULL, 10);
		if(ctx.runid == -1)
			continue;

		ctx.name = name;
		ctx.dir = de->d_name;

		ret = pred(&ctx, context);

		switch(ret) {
		case EXP_WALK_RETURN:
			exp = experiment_load_dir(root->exp_root, ctx.runid, de->d_name);
			strncpy(exp->exp_name, name, EXPNAMELEN);
			/* FALLTHROUGH */
		case EXP_WALK_BREAK:
			plat_closedir(dir);
			return exp;
		}
	}

	plat_closedir(dir);

	return NULL;
}

/**
 * Find node inside experiment config (or its subnode) using simple notation
 * Node identified by node names, delimited by semicolons, i.e.
 *   `workloads:test1:rqsched_params:scope`
 *
 * @note Node name lengths are limited to 256 bytes (PATHPARTMAXLEN)
 *
 * @param node starting node. Usually set to exp->exp_config
 * @param name path to node
 * @param parent pointer where pointer to parent node would be set. \
 * 				May be set to NULL
 *
 * @return Returns found node or NULL if nothing was found
 */
JSONNODE* experiment_cfg_find(JSONNODE* node, const char* name, JSONNODE** parent) {
	JSONNODE_ITERATOR i_iter, i_end;

	char node_name[PATHPARTMAXLEN];
	const char* next = NULL;
	const char* prev = name;

	int itemid;

	size_t length = 0;

	/* TODO: wl and tp aliases */

	do {
		next = strchr(prev, ':');

		if(parent) {
			*parent = node;
		}

		if(next == NULL) {
			strncpy(node_name, prev, PATHPARTMAXLEN);
		}
		else {
			length = next - prev;
			assert(length < PATHPARTMAXLEN);

			strncpy(node_name, prev, length);
			node_name[length] = '\0';
		}

		if(json_type(node) == JSON_NODE) {
			i_end = json_end(node);
			i_iter = json_find(node, node_name);

			if(i_iter == i_end)
				return NULL;

			node = *i_iter;
		}
		else if(json_type(node) == JSON_ARRAY) {
			itemid = strtol(node_name, NULL, 10);

			if(itemid == -1)
				return NULL;

			node = json_at(node, itemid);
		}
		else {
			node = NULL;
		}

		prev = next + 1;
	} while(next != NULL && node != NULL);

	return node;
}

int experiment_cfg_walk_impl(JSONNODE* parent, experiment_cfg_walk_func pre, experiment_cfg_walk_func post,
							 void* context, const char* parent_path, JSONNODE** child) {
	char node_path[PATHMAXLEN];
	size_t length = 0;
	int itemid = 0;

	JSONNODE_ITERATOR i_iter, i_end;
	JSONNODE* node;
	char* name;

	int ret;

	if(parent_path) {
		length = snprintf(node_path, PATHMAXLEN, "%s:", parent_path);
	}

	for(i_iter = json_begin(parent), i_end = json_end(parent);
			i_iter != i_end; ++i_iter) {
		node = *i_iter;

		if(json_type(parent) == JSON_NODE) {
			name = json_name(node);
			strncpy(node_path + length, name, PATHMAXLEN - length);
			json_free(name);
		}
		else if(json_type(parent) == JSON_ARRAY) {
			snprintf(node_path + length, PATHMAXLEN - length, "%d", itemid);
		}

		ret = pre(node_path, parent, node, context);

		if(ret == EXP_WALK_CONTINUE &&
				(json_type(node) == JSON_ARRAY || json_type(node) == JSON_NODE)) {
			ret = experiment_cfg_walk_impl(node, pre, post, context, node_path, child);
		}

		if(ret == EXP_WALK_CONTINUE && post) {
			ret = post(node_path, parent, node, context);
		}

		switch(ret) {
		case EXP_WALK_RETURN:
			*child = node;
			/* FALLTHROUGH */
		case EXP_WALK_BREAK:
			return EXP_WALK_BREAK;
		}

		++itemid;
	}

	return EXP_WALK_CONTINUE;
}

/**
 * Walk over experiment config.
 *
 * Walker should return one of EXP_WALK_* values.
 *
 * @param config pointer to config root node. Should be exp->exp_config
 * @param pre function that will be called before walking over array or node
 * @param post function that will be called after walking over array or node. \
 *             May be set to NULL
 * @param context passed into each call of walker
 *
 * @return If walker returns EXP_WALK_RETURN, node which triggered it, or NULL.
 */
JSONNODE* experiment_cfg_walk(JSONNODE* config, experiment_cfg_walk_func pre,
		                      experiment_cfg_walk_func post, void* context) {
	JSONNODE* child = NULL;

	experiment_cfg_walk_impl(config, pre, post, context, NULL, &child);

	return child;
}

/**
 * Set value of node inside experiment config
 *
 * @param config root node of config. Should be exp->exp_config
 * @param name path to node
 * @param value string representation of value (it converts to node's type)
 *
 * @return EXP_CONFIG_OK or one of EXP_CONFIG_* errors.
 */
int experiment_cfg_set(JSONNODE* config, const char* name, const char* value) {
	JSONNODE* node = experiment_cfg_find(config, name, NULL);

	if(node == NULL)
		return EXP_CONFIG_NOT_FOUND;

	switch(json_type(node)) {
	case JSON_STRING:
		json_set_a(node, value);
		return EXP_CONFIG_OK;
	case JSON_NUMBER:
		/* Act like libjson: provide both numbers (integer & float),
		 * Than consumer of this node call json_as_i or json_as_f and
		 * get correct value */
		json_set_i(node, strtoll(value, NULL, 10));
		json_set_f(node, atof(value));
		return EXP_CONFIG_OK;
	case JSON_BOOL:
		if(strcmp(value, "true") == 0) {
			json_set_b(node, B_TRUE);
		}
		else if(strcmp(value, "false") == 0) {
			json_set_b(node, B_FALSE);
		}
		else {
			return EXP_CONFIG_INVALID_VALUE;
		}
		return EXP_CONFIG_OK;
	}

	return EXP_CONFIG_INVALID_NODE;
}

/**
 * Add JSON node to a config
 *
 * @param config root node of config. Should be exp->exp_config
 * @param parent_name name of parent node. May be set to NULL, in that case config would be used as parent node
 * @param node node to be inserted
 * @param replace replace node if duplicate exists.
 *
 * @return EXP_CONFIG_OK or one of EXP_CONFIG_* errors.
 */
int experiment_cfg_add(JSONNODE* config, const char* parent_name, JSONNODE* node, boolean_t replace) {
	JSONNODE* parent = config;
	JSONNODE_ITERATOR i_iter, i_end;
	char* name;

	if(parent_name) {
		parent = experiment_cfg_find(config, parent_name, NULL);

		if(parent == NULL)
			return EXP_CONFIG_NOT_FOUND;
	}

	if(json_type(parent) != JSON_NODE) {
		return EXP_CONFIG_INVALID_NODE;
	}

	/* Check if node not yet exists */
	name = json_name(node);
	i_end = json_end(parent);
	i_iter = json_find(parent, name);
	json_free(name);

	if(i_iter != i_end) {
		if(!replace)
			return EXP_CONFIG_DUPLICATE;

		json_erase(parent, i_iter);
	}

	json_push_back(parent, node);

	return EXP_CONFIG_OK;
}

/* ---------------------------
 * experiment_process_config()
 * --------------------------- */

#define EXP_CONFIG_NONE					0
#define EXP_CONFIG_THREADPOOLS			1
#define EXP_CONFIG_WORKLOADS			2
#define EXP_CONFIG_STEPS				3
#define EXP_CONFIG_WL_CHAIN				4

struct exp_config_context {
	experiment_t* exp;

	int mode;
	JSONNODE* node;

	int error;

	union {
		exp_threadpool_t* tp;
		exp_workload_t* wl;
	} data;
	boolean_t add;
};

static int exp_tp_proc_param(const char* node_name, JSONNODE* node, struct exp_config_context* ctx) {
	 exp_threadpool_t* etp = ctx->data.tp;

	 /**
	  * We don't any additional checks of node type here.
	  * If json_as_int find non-number node, it will return 0,
	  * which would be recognised by tsload layer as error
	  * (but error messages may be confusing).  */

	if(strcmp(node_name, "num_threads") == 0) {
		etp->tp_num_threads = json_as_int(node);
	}
	else if(strcmp(node_name, "quantum") == 0) {
		etp->tp_quantum = json_as_int(node);
	}
	else if(strcmp(node_name, "discard") == 0) {
		etp->tp_discard = json_as_bool(node);
	}
	else if(strcmp(node_name, "disp") == 0) {
		etp->tp_disp = node;
		return EXP_WALK_NEXT;
	}
	else if(strcmp(node_name, "sched") == 0) {
		etp->tp_sched = node;
		return EXP_WALK_NEXT;
	}

	return EXP_WALK_CONTINUE;
}

static int exp_wl_proc_param(const char* node_name, JSONNODE* node, struct exp_config_context* ctx) {
	exp_workload_t* ewl = ctx->data.wl;
	char* str = NULL;

	if(strcmp(node_name, "wltype") == 0) {
		str = json_as_string(node);
		strncpy(ewl->wl_type, str, WLTNAMELEN);
		json_free(str);
	}
	else if(strcmp(node_name, "workload") == 0) {
		if(ctx->mode == EXP_CONFIG_WL_CHAIN) {
			str = json_as_string(node);
			strncpy(ewl->wl_chain_name, str, WLTNAMELEN);
			json_free(str);
		}
	}
	else if(strcmp(node_name, "threadpool") == 0) {
		str = json_as_string(node);
		strncpy(ewl->wl_tp_name, str, WLTNAMELEN);
		json_free(str);
	}
	else if(strcmp(node_name, "deadline") == 0) {
		ewl->wl_deadline = json_as_int(node);
	}
	else if(strcmp(node_name, "rqsched") == 0) {
		ewl->wl_rqsched = node;
		return EXP_WALK_NEXT;
	}
	else if(strcmp(node_name, "params") == 0) {
		ewl->wl_params = node;
		return EXP_WALK_NEXT;
	}
	else if(strcmp(node_name, "chain") == 0) {
		ewl->wl_chain = node;
		ctx->mode = EXP_CONFIG_WL_CHAIN;

		ewl->wl_is_chained = B_TRUE;
	}

	return EXP_WALK_CONTINUE;
}

static int exp_cfg_proc_walk_pre(const char* name, JSONNODE* parent, JSONNODE* node, void* context) {
	struct exp_config_context* ctx = (struct exp_config_context*) context;
	char* node_name = strrchr_y(name, ':');

	if(node_name == NULL) {
		node_name = name;
	}
	else {
		++node_name;
	}

	if(strcmp(name, "name") == 0) {
		char* str = json_as_string(node);
		strncpy(ctx->exp->exp_name, str, EXPNAMELEN);
		json_free(str);
	}
	else if(strcmp(name, "threadpools") == 0) {
		ctx->mode = EXP_CONFIG_THREADPOOLS;
	}
	else if(strcmp(name, "workloads") == 0) {
		ctx->mode = EXP_CONFIG_WORKLOADS;
	}
	else if(strcmp(name, "steps") == 0) {
		ctx->mode = EXP_CONFIG_STEPS;
	}
	else if(ctx->mode != EXP_CONFIG_NONE) {
		if(ctx->node == NULL) {
			ctx->node = node;

			if(ctx->mode == EXP_CONFIG_THREADPOOLS) {
				ctx->data.tp = exp_tp_create(node_name);
				ctx->add = B_TRUE;
			}
			else {
				 /* Workloads are defined inside two subnodes: steps and workloads
				  * So check if workload already have been created */
				 exp_workload_t* ewl = hash_map_find(ctx->exp->exp_workloads, node_name);

				 if(ewl == NULL) {
					 ewl = exp_wl_create(node_name);
					 ctx->add = B_TRUE;
				 }
				 else {
					 ctx->add = B_FALSE;
				 }

				 if(ctx->mode == EXP_CONFIG_STEPS) {
					 ewl->wl_steps_cfg = node;
				 }

				 ctx->data.wl = ewl;
			}
		}
		else {
			if(ctx->mode == EXP_CONFIG_THREADPOOLS) {
				return exp_tp_proc_param(node_name, node, ctx);
			}
			else if(ctx->mode == EXP_CONFIG_WORKLOADS || ctx->mode == EXP_CONFIG_WL_CHAIN) {
				return exp_wl_proc_param(node_name, node, ctx);
			}
		}
	}

	return EXP_WALK_CONTINUE;
}

static int exp_cfg_proc_walk_post(const char* name, JSONNODE* parent, JSONNODE* node, void* context) {
	struct exp_config_context* ctx = (struct exp_config_context*) context;
	char* node_name = strrchr_y(name, ':');

	if(node_name == NULL) {
		node_name = name;
	}
	else {
		++node_name;
	}

	if(ctx->node == NULL) {
		ctx->mode = EXP_CONFIG_NONE;
	}
	else if(ctx->node == node) {
		if(ctx->mode == EXP_CONFIG_THREADPOOLS) {
			if(hash_map_insert(ctx->exp->exp_threadpools, ctx->data.tp) != HASH_MAP_OK) {
				ctx->error = EXP_CONFIG_DUPLICATE;
				return EXP_WALK_BREAK;
			}
			ctx->data.tp = NULL;
		}
		else {
			if(ctx->add) {
				if(hash_map_insert(ctx->exp->exp_workloads, ctx->data.wl) != HASH_MAP_OK) {
					ctx->error = EXP_CONFIG_DUPLICATE;
					return EXP_WALK_BREAK;
				}
			}
			ctx->data.wl = NULL;
		}
		ctx->node = NULL;
	}

	if(ctx->mode == EXP_CONFIG_WL_CHAIN && strcmp(node_name, "chain") == 0)  {
		ctx->mode = EXP_CONFIG_WORKLOADS;
	}

	return EXP_WALK_CONTINUE;
}

int exp_wl_chain_walker(hm_item_t* obj, void* context) {
	exp_workload_t* ewl = (exp_workload_t*) obj;
	exp_workload_t* parent;
	struct exp_config_context* ctx = (struct exp_config_context*) context;

	if(ewl->wl_is_chained) {
		parent = hash_map_find_nolock(ctx->exp->exp_workloads, ewl->wl_chain_name);

		if(parent == NULL) {
			ctx->error = EXP_CONFIG_NOT_FOUND;
			return HM_WALKER_STOP;
		}

		if(parent->wl_chain_next != NULL) {
			ctx->error = EXP_CONFIG_DUPLICATE;
			return HM_WALKER_STOP;
		}

		parent->wl_chain_next = ewl;
	}

	return HM_WALKER_CONTINUE;
}

/**
 * Processes experiment config and fetches workloads and threadpools into
 * experiment object.
 *
 * @return EXP_CONFIG_OK if everything went fine
 */
int experiment_process_config(experiment_t* exp) {
	struct exp_config_context ctx;

	ctx.exp = exp;
	ctx.mode = EXP_CONFIG_NONE;
	ctx.node = NULL;
	ctx.error = EXP_CONFIG_OK;
	ctx.data.wl = NULL;
	ctx.add = B_FALSE;

	exp->exp_workloads = hash_map_create(&exp_workload_hash_map, "wl-%s", exp->exp_name);
	exp->exp_threadpools = hash_map_create(&exp_tp_hash_map, "tp-%s", exp->exp_name);

	experiment_cfg_walk(exp->exp_config, exp_cfg_proc_walk_pre, exp_cfg_proc_walk_post, &ctx);

	if(ctx.error == EXP_CONFIG_OK) {
		hash_map_walk(exp->exp_workloads, exp_wl_chain_walker, &ctx);
	}

	return ctx.error;
}

/* ---------------------------
 * experiment_open_workloads()
 * --------------------------- */

struct exp_wl_open_context {
	int flags;
	int error;
	experiment_t* exp;
};

tsfile_schema_t* exp_wl_generate_schema(experiment_t* exp, exp_workload_t* ewl, int flags) {
	tsfile_schema_t* schema = NULL;
	tsfile_field_t* field;
	wl_type_t* wlt;
	wlp_descr_t* wlp;
	int rq_param_count;
	int field_count, fid;
	size_t rqe_size = sizeof(exp_request_entry_t);

	if((flags & EXP_OPEN_SCHEMA_READ) &&
			((flags & EXP_OPEN_CREATE) == 0)) {
		char schema_path[PATHMAXLEN];
		char schema_filename[PATHPARTMAXLEN];

		snprintf(schema_filename, PATHPARTMAXLEN, "%s-%s", ewl->wl_name, EXP_SCHEMA_SUFFIX);

		path_join(schema_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, schema_filename, NULL);

		return tsfile_schema_read(schema_path);
	}

	wlt = tsload_walk_workload_types(TSLOAD_WALK_FIND, ewl->wl_type, NULL);

	if(wlt == NULL)
		return NULL;

	/* Count number of per-request parameters */
	rq_param_count = 0;

	if(flags & EXP_OPEN_RQPARAMS) {
		wlp = &wlt->wlt_params[0];

		while(wlp->type != WLP_NULL) {
			if(wlp->flags & WLPF_REQUEST) {
				++rq_param_count;
			}
			++wlp;
		}
	}

	/* Clone per-request schema with additional fields */
	schema = tsfile_schema_clone(rq_param_count, &request_schema);
	if(schema == NULL)
		return NULL;

	/* Now fill in information about per-request params fields
	 * Update schema so it will know about per-request params:
	 * convert wlp_descr_t into tsfile_field_t
	 *
	 * Each request is serialized into TS file like:
	 * +-----------------------+------------+
	 * |  exp_request_entry_t  |  rq_params |
	 * +-----------------------+------------+
	 * rq_params are written as raw data.
	 *
	 * See also at tse_run_report_request() function
	 */
	if(rq_param_count > 0) {
		schema->hdr.entry_size += wlt->wlt_rqparams_size;
		schema->hdr.count += rq_param_count;
		wlp = &wlt->wlt_params[0];
		fid = request_schema.hdr.count;

		while(wlp->type != WLP_NULL) {
			if(wlp->flags & WLPF_REQUEST) {
				field = &schema->fields[fid];

				strncpy(field->name, wlp->name, MAXFIELDLEN);
				field->offset = wlp->off + sizeof(exp_request_entry_t);

				switch(wlp_get_base_type(wlp)) {
				case WLP_BOOL:
					field->type = TSFILE_FIELD_BOOLEAN;
					field->size = sizeof(wlp_bool_t);
					break;
				case WLP_INTEGER:
					field->type = TSFILE_FIELD_INT;
					field->size = sizeof(wlp_integer_t);
					break;
				case WLP_FLOAT:
					field->type = TSFILE_FIELD_FLOAT;
					field->size = sizeof(wlp_float_t);
					break;
				case WLP_RAW_STRING:
					field->type = TSFILE_FIELD_STRING;
					field->size = wlp->range.str_length;
					break;
				case WLP_STRING_SET:
					field->type = TSFILE_FIELD_INT;
					field->size = sizeof(wlp_strset_t);
					break;
				case WLP_HI_OBJECT:
					field->type = TSFILE_FIELD_INT;
					/* XXX: Actually not serializable */
					field->size = sizeof(wlp_hiobject_t);
					break;
				}

				++fid;
			}
			++wlp;
		}
	}

	return schema;
}

int exp_wl_open_walker(hm_item_t* obj, void* context) {
	struct exp_wl_open_context * ctx = (struct exp_wl_open_context*) context;
	exp_workload_t* ewl = (exp_workload_t*) obj;

	tsfile_t* tsfile;
	tsfile_schema_t* schema = exp_wl_generate_schema(ctx->exp, ewl, ctx->flags);

	char tsf_path[PATHMAXLEN];
	char tsf_filename[PATHPARTMAXLEN];

	if(schema == NULL) {
		ctx->error = EXP_OPEN_BAD_SCHEMA;
		return HM_WALKER_STOP;
	}

	snprintf(tsf_filename, PATHPARTMAXLEN, "%s.%s", ewl->wl_name, TSFILE_EXTENSION);
	path_join(tsf_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, tsf_filename, NULL);

	if(ctx->flags & EXP_OPEN_CREATE) {
		char schema_path[PATHMAXLEN];
		char schema_filename[PATHPARTMAXLEN];

		snprintf(schema_filename, PATHPARTMAXLEN, "%s-%s", ewl->wl_name, EXP_SCHEMA_SUFFIX);
		path_join(schema_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, schema_filename, NULL);

		if(tsfile_schema_write(schema_path, schema) != 0) {
			mp_free(schema);
			ctx->error = EXP_OPEN_ERROR_SCHEMA;
			return HM_WALKER_STOP;
		}

		tsfile = tsfile_create(tsf_path, schema);
	}
	else {
		tsfile = tsfile_open(tsf_path, schema);
	}

	if(tsfile == NULL) {
		mp_free(schema);
		ctx->error = EXP_OPEN_ERROR_TSFILE;
		return HM_WALKER_STOP;
	}

	ewl->wl_file = tsfile;
	ewl->wl_file_schema = schema;
	ewl->wl_file_flags = ctx->flags;

	return HM_WALKER_CONTINUE;
}

/**
 * Loads workloads from files or creates files for them
 *
 * Supported flags are:
 *
 * @param exp experiment run (shouldn't be root experiment)
 * @param flags experiment flags
 */
int experiment_open_workloads(experiment_t* exp, int flags) {
	struct exp_wl_open_context ctx;

	if(exp->exp_workloads == NULL) {
		return EXP_OPEN_NO_WORKLOADS;
	}

	ctx.error = 0;
	ctx.exp = exp;
	ctx.flags = flags;

	hash_map_walk(exp->exp_workloads, exp_wl_open_walker, &ctx);

	return ctx.error;
}
