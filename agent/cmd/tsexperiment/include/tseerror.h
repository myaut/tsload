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

#ifndef TSEERROR_H_
#define TSEERROR_H_

#include <tsload/defs.h>

#include <experiment.h>


#define TSE_ERR_DEST_STDERR		0x1
#define TSE_ERR_DEST_EXPERIMENT	0x2
#define TSE_ERR_DEST_LOG		0x4

#define TSE_PRINT_ALL			(~0)
#define TSE_PRINT_NOLOG			(~TSE_ERR_DEST_LOG)

int tse_vprintf(int flags, const char* fmtstr, va_list va);
int tse_printf(int flags, const char* format, ...)
	CHECKFORMAT(printf, 2, 3);

int tse_experiment_error_msg(experiment_t* exp, unsigned experr, const char* format, ...)
	CHECKFORMAT(printf, 3, 4);

void tse_error_enable_dest(int dest);
void tse_error_disable_dest(int dest);
void tse_error_set_experiment(experiment_t* exp);

int tse_error_init(void);
void tse_error_fini(void);

#endif /* TSERROR_H_ */
