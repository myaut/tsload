
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

#include <tsload/mempool.h>
#include <tsload/dirent.h>
#include <tsload/pathutil.h>
#include <tsload/hashmap.h>
#include <tsload/time.h>
#include <tsload/filelock.h>
#include <tsload/posixdecl.h>

#include <tsload/json/json.h>

#include <tsload.h>
#include <tsload/load/wlparam.h>
#include <tsload/load/wltype.h>

#include <experiment.h>
#include <tseerror.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

unsigned exp_load_error = EXPERR_LOAD_OK;
unsigned exp_create_error = EXPERR_CREATE_OK;

static char* strrchr_y(const char* s, int c) {
	/* TODO: fallback to libc if possible. */
	const char* from = s + strlen(s);

    do {
        if(from == s)
            return NULL;

        --from;
    } while(*from != c);

    return (char*) from;
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
 * Checks that required experiment directories and files exist
 * and have applicable permissions
 *
 * @param path	path to file/directory
 * @param access_mask access mask for checking permissions
 * @param err_noent error that would be returned if file do not exist
 * @param err_perms error that would be returned if file have invalid permissions
 *
 * @note set err_noent/err_perms to EXPERIMENT_OK if you wish to ignore that check
 */
static int experiment_check_path(const char* path, int access_mask,
								 int err_noent, int err_perms) {
	struct stat statbuf;

	if(err_noent != EXPERIMENT_OK &&
			stat(path, &statbuf) == -1) {
		return err_noent;
	}

	if(err_perms != EXPERIMENT_OK &&
			access(path, access_mask) == -1) {
		return err_perms;
	}

	return EXPERIMENT_OK;
}

static boolean_t experiment_check_path_exists(const char* path) {
	struct stat statbuf;

	if(stat(path, &statbuf) == 0) {
		return B_TRUE;
	}

	return B_FALSE;
}

/**
 * Reads JSON config of experiment from file and parses it
 *
 * @return LOAD_OK if everything was OK or LOAD_ERR_ constant
 */
static int experiment_read_config(experiment_t* exp) {
	int ret = EXPERR_LOAD_OK;
	json_buffer_t* buf;

	int err;
	struct stat s;

	char experiment_directory[PATHMAXLEN];
	char experiment_filename[PATHMAXLEN];

	/* Run some preliminary checks */
	path_join(experiment_directory, PATHMAXLEN,
				  exp->exp_root, exp->exp_directory, NULL);
	ret = experiment_check_path(experiment_directory, R_OK | X_OK,
								EXPERR_LOAD_NO_DIR, EXPERR_LOAD_DIR_NO_PERMS);
	if(ret != EXPERIMENT_OK) {
		tse_experiment_error_msg(exp, ret, "Couldn't enter experiment directory '%s': "
								 "not exists or no permissions", experiment_directory);
		return ret;
	}

	path_join(experiment_filename, PATHMAXLEN,
					experiment_directory, EXPERIMENT_FILENAME, NULL);
	ret = experiment_check_path(experiment_filename, R_OK,
								EXPERR_LOAD_NO_FILE, EXPERR_LOAD_FILE_NO_PERMS);
	if(ret != EXPERIMENT_OK) {
		tse_experiment_error_msg(exp, ret, "Couldn't open experiment file '%s': "
								 "not exists or no permissions", experiment_filename);
		return ret;
	}

	/* No load file and parse JSON */
	buf = json_buf_from_file(experiment_filename);

	if(buf == NULL) {
		if(json_errno() != JSON_OK) {
			tse_experiment_error_msg(exp, EXPERR_LOAD_FILE_ERROR,
							"Couldn't open experiment file %s: %s",
							experiment_filename, json_error_message());
		}
		else {
			tse_experiment_error_msg(exp, EXPERR_LOAD_FILE_ERROR,
							"Couldn't open experiment file %s",
							experiment_filename);
		}
		return EXPERR_LOAD_FILE_ERROR;
	}

	err = json_parse(buf, &exp->exp_config);

	if(err != JSON_OK) {
		tse_experiment_error_msg(exp, EXPERR_LOAD_BAD_JSON,
				"JSON parse error of file %s: %s",
				experiment_filename, json_error_message());
		ret = EXPERR_LOAD_BAD_JSON;
	}

	return ret;
}

static void experiment_init(experiment_t* exp, const char* root_path, int runid, const char* dir) {
	strcpy(exp->exp_root, root_path);
	strncpy(exp->exp_directory, dir, PATHPARTMAXLEN);

	aas_init(&exp->exp_name);
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

	aas_copy(aas_init(&etp->tp_name), name);

	etp->tp_discard = B_FALSE;
	etp->tp_quantum = 0;
	etp->tp_disp = NULL;
	etp->tp_sched = NULL;

	etp->tp_next = NULL;

	etp->tp_status = EXPERIMENT_NOT_CONFIGURED;

	return etp;
}

static void exp_tp_destroy(exp_threadpool_t* etp) {
	aas_free(&etp->tp_name);

	mp_free(etp);
}

int exp_tp_destroy_walker(hm_item_t* obj, void* ctx) {
	mp_free(obj);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

static exp_workload_t* exp_wl_create(const char* name) {
	 exp_workload_t* ewl = mp_malloc(sizeof(exp_workload_t));

	 aas_copy(aas_init(&ewl->wl_name), name);

	 aas_init(&ewl->wl_type);
	 aas_init(&ewl->wl_tp_name);
	 aas_init(&ewl->wl_chain_name);

	 ewl->wl_deadline = TS_TIME_MAX;

	 ewl->wl_rqsched = NULL;
	 ewl->wl_params = NULL;
	 ewl->wl_chain = NULL;

	 ewl->wl_chain_next = NULL;
	 ewl->wl_next = NULL;

	 ewl->wl_steps = NULL;
	 ewl->wl_steps_cfg = NULL;

	 ewl->wl_tp = NULL;

	 ewl->wl_is_chained = B_FALSE;

	 ewl->wl_file_schema = NULL;
	 ewl->wl_file = NULL;
	 ewl->wl_file_flags = EXP_OPEN_NONE;
	 ewl->wl_file_index = 0;

	 ewl->wl_status = EXPERIMENT_UNKNOWN;

	 return ewl;
}

static void exp_wl_destroy(exp_workload_t* ewl) {
	if(ewl->wl_file != NULL) {
		tsfile_close(ewl->wl_file);
	}

	if(ewl->wl_file_schema != NULL) {
		mp_free(ewl->wl_file_schema);
	}
	aas_free(&ewl->wl_name);
	aas_free(&ewl->wl_type);
	aas_free(&ewl->wl_tp_name);
	aas_free(&ewl->wl_chain_name);

	mp_free(ewl);
}

int exp_wl_destroy_walker(hm_item_t* obj, void* ctx) {
	exp_workload_t* ewl = (exp_workload_t*) obj;

	exp_wl_destroy(ewl);

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
	json_node_t* status;
	json_node_t* name;

	experiment_init(exp, root_path, runid, dir);

	exp_load_error = experiment_read_config(exp);
	if(exp_load_error != EXPERR_LOAD_OK) {
		mp_free(exp);
		return NULL;
	}

	/* Fetch status & name if possible */
	status = experiment_cfg_find(exp->exp_config, "status", NULL, JSON_NUMBER_INTEGER);
	if(status != NULL) {
		exp->exp_status = json_as_integer(status);
	}

	name = experiment_cfg_find(exp->exp_config, "name", NULL, JSON_STRING);
	if(name != NULL) {
		aas_copy(&exp->exp_name, json_as_string(name));
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

static void experiment_load_run_noent(void* context) {
	exp_load_error = EXPERR_LOAD_NO_DIR;
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
	return experiment_walk(root, experiment_load_run_walk, &runid,
						   experiment_load_run_noent);
}

/**
 * Returns an error occured in experiment_load_*() functions
 */
unsigned experiment_load_error() {
	return exp_load_error;
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
static long experiment_generate_id(experiment_t* root, boolean_t use_file) {
	FILE* runid_file;
	long runid = -1;

	char runid_path[PATHMAXLEN];
	char runid_str[32];

	path_join(runid_path, PATHMAXLEN, root->exp_root, EXPID_FILENAME, NULL);

	runid_file = fopen(runid_path, "r");
	if(runid_file != NULL) {
		plat_flock(fileno(runid_file), LOCK_EX);
		
		if(use_file) {
			fgets(runid_str, 32, runid_file);
			runid = strtol(runid_str, NULL, 10);
		}
	}

	if(runid == -1) {
		experiment_walk(root, experiment_max_runid_walk, &runid, NULL);
	}
	++runid;

	if(runid_file != NULL) {
		runid_file = freopen(runid_path, "w", runid_file);
		if(runid_file == NULL)
			return -1;
	}
	else {
		runid_file = fopen(runid_path, "w");
		if(runid_file == NULL)
			return -1;

		plat_flock(fileno(runid_file), LOCK_EX);
	}

	fprintf(runid_file, "%ld\n", runid);

	plat_flock(fileno(runid_file), LOCK_UN);
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
	long runid = experiment_generate_id(root, B_TRUE);
	experiment_t* exp = NULL;
	char dir[PATHPARTMAXLEN];

	const char* cfg_name = NULL;
	json_node_t* j_name;

	if(runid == -1) {
		tse_experiment_error_msg(root, EXPERR_CREATE_ALLOC_RUNID,
								 "Couldn't allocate runid for experiment: errno %d!", errno);
		exp_create_error = EXPERR_CREATE_ALLOC_RUNID;
		return NULL;
	}

	if(base == NULL)
		base = root;

	if(name == NULL) {
		j_name = json_find_opt(base->exp_config, "name");
		if(j_name == NULL) {
			tse_experiment_error_msg(root, EXPERR_CREATE_UNNAMED,
									 "Neither command-line name nor name from config was provided");
			exp_create_error = EXPERR_CREATE_UNNAMED;
			return NULL;
		}

		name = cfg_name = json_as_string(j_name);
	}

	snprintf(dir, PATHPARTMAXLEN, "%s-%ld", name, runid);

	exp = mp_malloc(sizeof(experiment_t));
	experiment_init(exp, root->exp_root, runid, dir);

	strcpy(exp->exp_basedir, base->exp_directory);
	if(name)
		aas_copy(&exp->exp_name, name);

	exp->exp_config = json_copy_node(base->exp_config);

	experiment_cfg_add(exp->exp_config, NULL, JSON_STR("name"),
					   json_new_string(json_str_create(name)), B_TRUE);

	return exp;
}

/**
 * Returns an error occured in experiment_create() function
 */
unsigned experiment_create_error() {
	return exp_create_error;
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
		json_node_destroy(exp->exp_config);
	}

	if(exp->exp_log != NULL) {
		fclose(exp->exp_log);
	}

	cv_destroy(&exp->exp_cv);
	mutex_destroy(&exp->exp_mutex);

	aas_free(&exp->exp_name);

	mp_free(exp);
}

/**
 * Tries to create experiment subdirectory
 */
int experiment_mkdir(experiment_t* exp, experiment_t* root) {
	char path[PATHMAXLEN];

	int ret;
	
	boolean_t retried = B_FALSE;
	
	if(exp->exp_runid == EXPERIMENT_ROOT) {
		tse_experiment_error_msg(exp, EXPERR_MKDIR_IS_ROOT, "Failed to create experiment's run, "
						 "cannot be performed on root");
		return EXPERR_MKDIR_IS_ROOT;
	}

	ret = experiment_check_path(exp->exp_root, R_OK | W_OK | X_OK,
								EXPERIMENT_OK, EXPERR_MKDIR_NO_PERMS);
	if(ret != EXPERIMENT_OK) {
		tse_experiment_error_msg(exp, ret, "Failed to create experiment's run: "
						 	 	 "no permissions on '%s'", exp->exp_root);
		return ret;
	}

retry:
	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);

	if(experiment_check_path_exists(path)) {
		/* Possibly a stale or invalid id file, try to re-generate it */
		int runid;
		
		if(!retried) {
			runid = experiment_generate_id(root, B_FALSE);
			
			if(runid != -1) {
				retried = B_TRUE;
				
				snprintf(exp->exp_directory, PATHPARTMAXLEN, "%s-%d", exp->exp_name, runid);
				exp->exp_runid = runid;
				
				goto retry;
			}
		}
				
		tse_experiment_error_msg(exp, EXPERR_MKDIR_EXISTS,
				"Failed to create experiment's run '%s': already exists", path);
		return EXPERR_MKDIR_EXISTS;
	}

	if(mkdir(path, S_IRWXU |
				   S_IRGRP | S_IXGRP |
				   S_IROTH | S_IXOTH) != 0) {
		tse_experiment_error_msg(exp, EXPERR_MKDIR_ERROR,
				"Failed to create experiment's run '%s': mkdir error %d", path, errno);
		return EXPERR_MKDIR_ERROR;
	}

	return EXPERR_MKDIR_OK;
}

/**
 * Write experiment to file experiment.json in it's subdir
 */
int experiment_write(experiment_t* exp) {
	char path[PATHMAXLEN];
	char old_path[PATHMAXLEN];

	FILE* file;
	char* data;

	int ret;

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);
	ret = experiment_check_path(path, R_OK | W_OK | X_OK,
								EXPERR_WRITE_NO_DIR, EXPERR_WRITE_DIR_NO_PERMS);
	if(ret != 0) {
		tse_experiment_error_msg(exp, ret,
				"Failed to write experiment's config to '%s': no permissions or not exists", path);
		return ret;
	}

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_FILENAME, NULL);
	path_join(old_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_FILENAME_OLD, NULL);

	if(experiment_check_path_exists(path)) {
		if(rename(path, old_path) == -1) {
			tse_experiment_error_msg(exp, EXPERR_WRITE_RENAME_ERROR, "Failed to rename experiment's config "
							  "'%s' -> '%s'. Errno: %d", path, old_path, errno);
			return EXPERR_WRITE_RENAME_ERROR;
		}
	}

	file = fopen(path, "w");
	if(file == NULL) {
		tse_experiment_error_msg(exp, EXPERR_WRITE_FILE_ERROR, "Failed to write experiment's config '%s':"
						 "cannot open, errno: %d", path, errno);

		rename(old_path, path);
		return EXPERR_WRITE_FILE_ERROR;
	}

	if(json_write_file(exp->exp_config, file, B_TRUE) != JSON_OK) {
		tse_experiment_error_msg(exp, EXPERR_WRITE_FILE_JSON_FAIL, "Failed to write experiment's config '%s': "
						 "JSON error %s", path, json_error_message());

		return EXPERR_WRITE_FILE_JSON_FAIL;
	}

	fclose(file);

	return EXPERR_WRITE_OK;
}

/**
 * Opens experiment log
 */
int experiment_open_log(experiment_t* exp) {
	char path[PATHMAXLEN];
	int ret;

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		tse_experiment_error_msg(exp, EXPERR_OPENLOG_IS_ROOT, "Failed to create experiment's log, "
						 "cannot be performed on root");
		return EXPERR_OPENLOG_IS_ROOT;
	}

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);
	ret = experiment_check_path(path, R_OK | W_OK | X_OK,
								EXPERR_OPENLOG_NO_DIR, EXPERR_OPENLOG_DIR_NO_PERMS);
	if(ret != 0) {
		tse_experiment_error_msg(exp, ret,
				"Failed to create experiment's log in '%s': no permissions or not exists",
				path);
		return ret;
	}

	path_join(path, PATHMAXLEN, exp->exp_root, exp->exp_directory, EXPERIMENT_LOG_FILE, NULL);

	if(experiment_check_path_exists(path)) {
		tse_experiment_error_msg(exp, EXPERR_OPENLOG_EXISTS, "Failed to create experiment's log '%s': "
						 "log already exists", path);
		return EXPERR_OPENLOG_EXISTS;
	}

	exp->exp_log = fopen(path, "w");

	if(exp->exp_log == NULL) {
		tse_experiment_error_msg(exp, EXPERR_OPENLOG_ERROR, "Failed to create experiment's log '%s': "
						 "cannot open, errno: %d", path, errno);
		return EXPERR_OPENLOG_ERROR;
	}

	return EXPERR_OPENLOG_OK;
}

/**
 * Walk over experiment runs and call pred() for each run
 *
 * pred() should return one of EXP_WALK_* values
 *
 * @note if run contains subruns inside it, experiment_walk() will ignore them
 */
experiment_t* experiment_walk(experiment_t* root, experiment_walk_func pred, void* context, 
							  experiment_noent_func noent) {
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
			if(exp)
				aas_copy(&exp->exp_name, name);
			/* FALLTHROUGH */
		case EXP_WALK_BREAK:
			plat_closedir(dir);
			return exp;
		}
	}

	if(noent)
		noent(context);
	
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
json_node_t* experiment_cfg_find(json_node_t* node, const char* name, json_node_t** parent, json_type_t type) {
	json_node_t* iter;

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
			if(next != NULL) {
				iter = json_find_opt(node, node_name);
			}
			else {
				iter = json_find(node, node_name);

				if(json_check_type(iter, type) != JSON_OK) {
					iter = NULL;
				}
			}

			if(iter == NULL)
				return NULL;

			node = iter;
		}
		else if(json_type(node) == JSON_ARRAY) {
			itemid = strtol(node_name, NULL, 10);

			if(itemid == -1)
				return NULL;

			node = json_getitem(node, itemid);
		}
		else {
			node = NULL;
		}

		prev = next + 1;
	} while(next != NULL && node != NULL);

	return node;
}

int experiment_cfg_walk_impl(json_node_t* parent, experiment_cfg_walk_func pre, experiment_cfg_walk_func post,
							 void* context, const char* parent_path, json_node_t** child) {
	char node_path[PATHMAXLEN];
	size_t length = 0;
	int itemid = 0;

	json_node_t* node;

	int ret;

	if(parent_path) {
		length = snprintf(node_path, PATHMAXLEN, "%s:", parent_path);
	}

	json_for_each(parent, node, itemid) {
		if(json_type(parent) == JSON_NODE) {
			strncpy(node_path + length, json_name(node), PATHMAXLEN - length);
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
json_node_t* experiment_cfg_walk(json_node_t* config, experiment_cfg_walk_func pre,
		                      experiment_cfg_walk_func post, void* context) {
	json_node_t* child = NULL;

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
int experiment_cfg_set(json_node_t* config, const char* name, const char* value) {
	json_node_t* node = experiment_cfg_find(config, name, NULL, JSON_ANY);

	if(node == NULL)
		return EXP_CONFIG_NOT_FOUND;

	switch(json_type(node)) {
	case JSON_STRING:
		json_set_string(node, json_str_create(value));
		return EXP_CONFIG_OK;
	case JSON_NUMBER:
		if(json_set_number(node, value))
			return EXP_CONFIG_INVALID_VALUE;
		return EXP_CONFIG_OK;
	case JSON_BOOLEAN:
		if(strcmp(value, "true") == 0) {
			json_set_boolean(node, B_TRUE);
		}
		else if(strcmp(value, "false") == 0) {
			json_set_boolean(node, B_FALSE);
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
int experiment_cfg_add(json_node_t* config, const char* parent_name, json_str_t name,
					   json_node_t* node, boolean_t replace) {
	json_node_t* parent = config;
	json_node_t* iter;

	if(parent_name) {
		parent = experiment_cfg_find(config, parent_name, NULL, JSON_ANY);

		if(parent == NULL)
			return EXP_CONFIG_NOT_FOUND;
	}

	if(json_type(parent) != JSON_NODE) {
		return EXP_CONFIG_INVALID_NODE;
	}

	/* Check if node not yet exists */
	iter = json_find_opt(parent, JSON_C_STR(name));

	if(iter != NULL) {
		if(!replace)
			return EXP_CONFIG_DUPLICATE;

		json_remove_node(parent, iter);
	}

	json_add_node(parent, name, node);

	return EXP_CONFIG_OK;
}

/* ---------------------------
 * experiment_process_config()
 * --------------------------- */

static int exp_tp_proc(json_node_t* node, exp_threadpool_t* etp) {
	if(json_get_integer_u(node, "num_threads", &etp->tp_num_threads) != JSON_OK)
		return -1;

	if(json_get_integer_tm(node, "quantum", &etp->tp_quantum) != JSON_OK)
		return -2;

	if(json_get_node(node, "disp", &etp->tp_disp) != JSON_OK)
		return -3;

	if(json_get_boolean(node, "discard", &etp->tp_discard) == JSON_INVALID_TYPE)
		return -4;

	if(json_get_array(node, "sched", &etp->tp_sched) == JSON_INVALID_TYPE)
		return -5;

	return 0;
}

static int exp_wl_proc(json_node_t* node, exp_workload_t* ewl) {
	int tp_error = 0;
	json_node_t* chain = NULL;

	if(json_get_string_aas(node, "wltype", &ewl->wl_type) != JSON_OK)
		return -1;

	tp_error = json_get_string_aas(node, "threadpool", &ewl->wl_tp_name);
	if(tp_error == JSON_INVALID_TYPE)
		return -2;

	if(json_get_node(node, "chain", &chain) == JSON_INVALID_TYPE)
		return -3;

	if(json_get_integer_tm(node, "deadline", &ewl->wl_deadline) == JSON_INVALID_TYPE)
		return -4;

	if(json_get_node(node, "params", &ewl->wl_params) != JSON_OK)
		return -5;

	if(chain != NULL) {
		if(json_get_string_aas(chain, "workload", &ewl->wl_chain_name) != JSON_OK)
				return -1;
		ewl->wl_chain = chain;
		ewl->wl_is_chained = B_TRUE;
	}
	else {
		if(tp_error != JSON_OK)
			return -2;

		if(json_get_node(node, "rqsched", &ewl->wl_rqsched) != JSON_OK)
			return -7;
	}

	return 0;
}

/**
 * Processes experiment config and fetches workloads and threadpools into
 * experiment object.
 *
 * @return EXP_CONFIG_OK if everything went fine
 */
int experiment_process_config(experiment_t* exp) {
	json_node_t *workloads, *threadpools, *steps;
	json_node_t *workload, *threadpool, *step;
	int id;

	int error = 0;
	int ret = EXPERR_PROC_OK;

	if(json_get_node(exp->exp_config, "workloads", &workloads) != JSON_OK) {
		ret = EXPERR_PROC_NO_WLS;
		goto bad_json;
	}

	if(json_get_node(exp->exp_config, "threadpools", &threadpools) != JSON_OK) {
		ret = EXPERR_PROC_NO_TPS;
		goto bad_json;
	}

	if(json_get_node(exp->exp_config, "steps", &steps) != JSON_OK) {
		ret = EXPERR_PROC_NO_STEPS;
		goto bad_json;
	}

	exp->exp_workloads = hash_map_create(&exp_workload_hash_map, "wl-%s", exp->exp_name);
	exp->exp_threadpools = hash_map_create(&exp_tp_hash_map, "tp-%s", exp->exp_name);

	json_for_each(threadpools, threadpool, id) {
		exp_threadpool_t* etp = exp_tp_create(json_name(threadpool));
		error = exp_tp_proc(threadpool, etp);

		if(error != 0) {
			tse_experiment_error_msg(exp, EXPERR_PROC_INVALID_TP,
					  "Failed to process experiment config: threadpool '%s': %s",
					  json_name(threadpool), json_error_message());

			return EXPERR_PROC_INVALID_TP;
		}

		if(hash_map_insert(exp->exp_threadpools, etp) != HASH_MAP_OK) {
			tse_experiment_error_msg(exp, EXPERR_PROC_DUPLICATE_TP,
					   "Failed to process experiment config: threadpool '%s' already exists",
					   json_name(threadpool));

			exp_tp_destroy(etp);
			return EXPERR_PROC_DUPLICATE_TP;
		}
	}

	json_for_each(workloads, workload, id) {
		exp_workload_t* ewl = exp_wl_create(json_name(workload));
		error = exp_wl_proc(workload, ewl);

		if(error != 0) {
			tse_experiment_error_msg(exp, EXPERR_PROC_INVALID_WL,
					   "Failed to process experiment config: workload '%s': %s",
					   json_name(workload), json_error_message());

			return EXPERR_PROC_INVALID_WL;
		}

		if(hash_map_insert(exp->exp_workloads, ewl) != HASH_MAP_OK) {
			tse_experiment_error_msg(exp, EXPERR_PROC_DUPLICATE_WL,
					  "Failed to process experiment config: workload '%s' already exists",
					  json_name(workload));

			exp_wl_destroy(ewl);
			return EXPERR_PROC_DUPLICATE_WL;
		}

		if(ewl->wl_tp_name != NULL) {
			exp_threadpool_t* tp = hash_map_find(exp->exp_threadpools, ewl->wl_tp_name);

			if(tp == NULL) {
				tse_experiment_error_msg(exp, EXPERR_PROC_WL_NO_TP,
							"Failed to process experiment config: "
							"workload '%s' has to be attached to threadpool '%s', "
						    "but it does not exist", ewl->wl_name, ewl->wl_tp_name);
				return EXPERR_PROC_WL_NO_TP;
			}

			ewl->wl_tp = tp;
		}
	}

	json_for_each(steps, step, id) {
		exp_workload_t* ewl = hash_map_find(exp->exp_workloads, json_name(step));

		if(ewl == NULL) {
			tse_experiment_error_msg(exp, EXPERR_PROC_NO_STEP_WL,
						"Failed to process experiment config: "
						"could not find workload for steps '%s'", json_name(step));

			return EXPERR_PROC_NO_STEP_WL;
		}

		ewl->wl_steps_cfg = step;
	}

	return ret;

bad_json:
	tse_experiment_error_msg(exp, ret,
			   "Failed to process experiment config: missing config node: %s",
			   json_error_message());

	return ret;
}

/* ---------------------------
 * experiment_open_workloads()
 * --------------------------- */

struct exp_wl_open_context {
	int flags;
	int error;
	experiment_t* exp;
};

static void exp_wl_generate_rqparams(tsfile_schema_t* schema, wl_type_t* wlt, int rq_param_count) {
	tsfile_field_t* field;
	wlp_descr_t* wlp;

	int field_count, fid;

	size_t rqe_size = sizeof(exp_request_entry_t);

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
}

static tsfile_schema_t* exp_wl_generate_schema(struct exp_wl_open_context* ctx, exp_workload_t* ewl) {
	tsfile_schema_t* schema = NULL;

	int rq_param_count = 0;
	wl_type_t* wlt;
	wlp_descr_t* wlp;

	experiment_t* exp = ctx->exp;
	int flags = ctx->flags;

	char schema_path[PATHMAXLEN];
	char schema_filename[PATHPARTMAXLEN];

	snprintf(schema_filename, PATHPARTMAXLEN, "%s-%s", ewl->wl_name, EXP_SCHEMA_SUFFIX);
	path_join(schema_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, schema_filename, NULL);

	/* Use preliminary saved schema description
	 * */
	if((flags & EXP_OPEN_SCHEMA_READ) &&
			((flags & EXP_OPEN_CREATE) == 0)) {
		ctx->error = experiment_check_path(schema_path, R_OK,
										   EXPERR_OPEN_NO_SCHEMA_FILE, EXPERR_OPEN_SCHEMA_NO_PERMS);
		if(ctx->error != EXPERIMENT_OK) {
			tse_experiment_error_msg(ctx->exp, ctx->error,
					"Error opening schema for workload '%s': "
					"file has no permissions or not exists", ewl->wl_name);
			return NULL;
		}

		schema = tsfile_schema_read(schema_path);
		if(schema == NULL) {
			ctx->error = EXPERR_OPEN_SCHEMA_OPEN_ERROR;
			tse_experiment_error_msg(ctx->exp, ctx->error,
					"Error opening schema for workload '%s': "
					"cannot read schema from it", ewl->wl_name);
		}

		return schema;
	}

	/* Generating schema - first check if it is already exists and we need to create it
	 * */
	if((flags & EXP_OPEN_CREATE) && experiment_check_path_exists(schema_path)) {
		tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_SCHEMA_EXISTS,
				"Error generating schema for workload '%s': "
				"schema-file already exists", ewl->wl_name);
		ctx->error = EXPERR_OPEN_SCHEMA_EXISTS;
		return NULL;
	}

	wlt = tsload_walk_workload_types(TSLOAD_WALK_FIND, ewl->wl_type, NULL);

	if(wlt == NULL) {
		tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_SCHEMA_NO_WLTYPE,
				"Error generating schema for workload '%s': "
				"couldn't find workload type '%s'", ewl->wl_name, ewl->wl_type);
		ctx->error = EXPERR_OPEN_SCHEMA_NO_WLTYPE;
		return NULL;
	}

	/* Count number of per-request parameters */
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
	if(schema == NULL) {
		tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_SCHEMA_CLONE_ERROR,
				"Error generating schema: for workload '%s': "
				 "failed to clone it", ewl->wl_name);
		ctx->error = EXPERR_OPEN_SCHEMA_CLONE_ERROR;
		return NULL;
	}

	exp_wl_generate_rqparams(schema, wlt, rq_param_count);

	if(tsfile_schema_write(schema_path, schema) != 0) {
		tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_SCHEMA_WRITE_ERROR,
				"Error generating schema: for workload '%s': "
				"failed to write file", ewl->wl_name);
		ctx->error = EXPERR_OPEN_SCHEMA_WRITE_ERROR;

		mp_free(schema);
		schema = NULL;
	}

	return schema;
}

int exp_wl_open_walker(hm_item_t* obj, void* context) {
	struct exp_wl_open_context * ctx = (struct exp_wl_open_context*) context;
	exp_workload_t* ewl = (exp_workload_t*) obj;

	tsfile_t* tsfile;
	tsfile_schema_t* schema = NULL;

	char tsf_path[PATHMAXLEN];
	char tsf_filename[PATHPARTMAXLEN];

	snprintf(tsf_filename, PATHPARTMAXLEN, "%s.%s", ewl->wl_name, TSFILE_EXTENSION);
	path_join(tsf_path, PATHMAXLEN, ctx->exp->exp_root, ctx->exp->exp_directory, tsf_filename, NULL);

	/* Check if tsfile exists */
	if(ctx->flags & EXP_OPEN_CREATE) {
		if(experiment_check_path_exists(tsf_path)) {
			ctx->error = EXPERR_OPEN_TSF_EXISTS;
			tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_TSF_EXISTS,
					"Failed to create tsfile for workload '%s': "
					"file has no permissions or not exists", ewl->wl_name);
			return HM_WALKER_STOP;
		}
	}
	else {
		ctx->error = experiment_check_path(tsf_path, R_OK,
										   EXPERR_OPEN_NO_TSF_FILE, EXPERR_OPEN_TSF_NO_PERMS);

		if(ctx->error != EXPERIMENT_OK) {
			tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_NO_TSF_FILE,
					"Failed to open tsfile for workload '%s': "
					"file has no permissions or not exists", ewl->wl_name);
			return HM_WALKER_STOP;
		}
	}

	/* Open or generate schema and tsfiles */
	schema = exp_wl_generate_schema(ctx, ewl);
	if(schema == NULL)
		return HM_WALKER_STOP;

	if(ctx->flags & EXP_OPEN_CREATE) {
		tsfile = tsfile_create(tsf_path, schema);
	}
	else {
		tsfile = tsfile_open(tsf_path, schema);
	}

	if(tsfile == NULL) {
		tse_experiment_error_msg(ctx->exp, EXPERR_OPEN_ERROR_TSFILE,
				"Failed to open or create tsfile for workload '%s': "
				"tsfile internal error", ewl->wl_name);

		mp_free(schema);
		ctx->error = EXPERR_OPEN_ERROR_TSFILE;

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
	char experiment_path[PATHMAXLEN];
	int ret;

	if(exp->exp_runid == EXPERIMENT_ROOT) {
		tse_experiment_error_msg(exp, EXPERR_OPEN_IS_ROOT,
				"Failed to open workloads, "
				"cannot be performed on root");
		return EXPERR_OPEN_IS_ROOT;
	}

	if(exp->exp_workloads == NULL) {
		tse_experiment_error_msg(exp, EXPERR_OPEN_NO_WORKLOADS,
				"Failed to open workloads, "
				"there are no workloads");
		return EXPERR_OPEN_NO_WORKLOADS;
	}

	path_join(experiment_path, PATHMAXLEN, exp->exp_root, exp->exp_directory, NULL);
	ret = experiment_check_path(experiment_path, R_OK | ((flags & EXP_OPEN_CREATE) ? W_OK : 0),
								EXPERR_OPEN_DIR_NO_PERMS, EXPERR_OPEN_NO_DIR);
	if(ret != EXPERIMENT_OK) {
		tse_experiment_error_msg(exp, EXPERR_OPEN_DIR_NO_PERMS,
				"Failed to open workloads in '%s', "
				"no permissions or not exists", experiment_path);
		return ret;
	}

	ctx.error = 0;
	ctx.exp = exp;
	ctx.flags = flags;

	hash_map_walk(exp->exp_workloads, exp_wl_open_walker, &ctx);

	return ctx.error;
}

