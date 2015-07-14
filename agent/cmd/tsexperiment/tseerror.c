/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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


#define LOG_SOURCE "experiment"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/autostring.h>

#include <tsload/json/json.h>

#include <tsload/obj/obj.h>

#include <tsload.h>

#include <tseerror.h>
#include <commands.h>

#include <stdarg.h>
#include <stdio.h>


int tse_error_flags = TSE_ERR_DEST_STDERR | TSE_ERR_DEST_LOG;

static thread_mutex_t	tse_output_lock;
experiment_t* tse_error_exp = NULL;

int tse_vprintf(int flags, const char* fmtstr, va_list va) {
	va_list vatmp;

	int ret = 0;

	flags = tse_error_flags & flags;

	mutex_lock(&tse_output_lock);

	if(flags & TSE_ERR_DEST_STDERR) {
		va_copy(vatmp, va);
		ret = vfprintf(stderr, fmtstr, vatmp);

		fflush(stderr);
	}

	if((flags & TSE_ERR_DEST_EXPERIMENT) &&
			tse_error_exp->exp_log != NULL) {
		va_copy(vatmp, va);
		ret = vfprintf(tse_error_exp->exp_log, fmtstr, vatmp);

		fflush(tse_error_exp->exp_log);
	}

	if(flags & TSE_ERR_DEST_LOG) {
		va_copy(vatmp, va);
		ret = logmsg_va(LOG_WARN, fmtstr, vatmp);
	}

	mutex_unlock(&tse_output_lock);

	return ret;
}

int tse_printf(int flags, const char* format, ...) {
	int ret;
	va_list va;

	va_start(va, format);
	ret = tse_vprintf(flags, format, va);
	va_end(va);

	return ret;
}

int tse_error_msg_va(const char* errclass, const char* codefmt, int code,
					  const char* format, va_list va) {
	int ret = 0;

	char* fmtstr = NULL;
	char* codestr = NULL;

	aas_printf(&codestr, codefmt, code);
	aas_printf(&fmtstr, "ERROR %s-%s: %s\n", errclass, codestr, format);

	if(tse_error_flags & TSE_ERR_DEST_EXPERIMENT) {
		mutex_lock(&tse_error_exp->exp_mutex);

		tse_error_exp->exp_status = EXPERIMENT_ERROR;
		tse_error_exp->exp_error = code;

		mutex_unlock(&tse_error_exp->exp_mutex);
	}

	tse_vprintf(TSE_PRINT_ALL, fmtstr, va);

	aas_free(&codestr);
	aas_free(&fmtstr);

	return ret;
}

void tse_tsload_error_msg(ts_errcode_t code, const char* format, ...) {
	va_list va;

	va_start(va, format);
	(void) tse_error_msg_va("TSLOAD", "%03d", code, format, va);
	va_end(va);
}

void tse_tsfile_error_msg(ts_errcode_t code, const char* format, ...) {
	va_list va;

	va_start(va, format);
	(void) tse_error_msg_va("TSFILE", "%03d", code, format, va);
	va_end(va);
}

int tse_tsobj_error_msg(int tsobj_errno, const char* format, va_list va) {
	int ret;

	if(tsobj_errno == JSON_NOT_FOUND) {
		/* Do not report annoying "NOT FOUND" errors
		 * They are checked by upper layers (experiment.c or TSLoad Core) and
		 * thus they use json_message() to pick error message */
		return 0;
	}

	/* Actually it is JSON error, but libtsload would initialize libtsobj
	 * and thus patch JSON error handler, so we need to go upstairs */
	ret = tse_error_msg_va("JSON", "%02d", -tsobj_errno, format, va);

	return ret;
}

int tse_experiment_error_msg(experiment_t* exp, unsigned experr, const char* format, ...) {
	int ret;
	va_list va;

	/* exp is currently ignored */

	va_start(va, format);
	ret = tse_error_msg_va("EXPERR", "%04x", experr, format, va);
	va_end(va);

	return ret;
}

int tse_command_error_msg(int code, const char* format, ...) {
	int ret;
	va_list va;

	va_start(va, format);
	ret = tse_error_msg_va("CMD", "%1d", -code, format, va);
	va_end(va);

	return ret;
}

void tse_error_enable_dest(int dest) {
	tse_error_flags |= dest;
}

void tse_error_disable_dest(int dest) {
	tse_error_flags &= ~dest;
}

void tse_error_set_experiment(experiment_t* exp) {
	tse_error_flags |= TSE_ERR_DEST_EXPERIMENT;
	tse_error_exp = exp;
}

int tse_experr_to_cmderr(unsigned experr) {
	if(experr & 0x80) {
		return CMD_GENERIC_ERROR;
	}

	return CMD_ERROR_BASE - ((int) (experr & 0x7f));
}

int tse_error_init(void) {
	mutex_init(&tse_output_lock, "output_lock");

	tsload_error_msg = tse_tsload_error_msg;
	tsfile_error_msg = tse_tsfile_error_msg;
	tsobj_error_msg = tse_tsobj_error_msg;

	return 0;
}

void tse_error_fini(void) {
	mutex_destroy(&tse_output_lock);
}


