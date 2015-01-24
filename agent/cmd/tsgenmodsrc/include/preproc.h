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

#ifndef PREPROC_H_
#define PREPROC_H_

#include <tsload/defs.h>

#include <stdio.h>

#include <tsload/list.h>

#define MODPP_OK					0
#define MODPP_READ_ERROR			-1
#define MODPP_UNCLOSED_DIRECTIVE	-2
#define MODPP_INVALID_DIRECTIVE		-3
#define MODPP_MISSING_VAR			-4
#define MODPP_VAR_NOT_EXISTS		-5
#define MODPP_INVALID_VAR			-6
#define MODPP_DEFINE_ERROR			-7

typedef enum modpp_state_type {
	MODPP_STATE_NORMAL,
	MODPP_STATE_VARNAME,
	MODPP_STATE_DEFINE,
	MODPP_STATE_ENDDEF,
	MODPP_STATE_IFDEF,
	MODPP_STATE_ELSE,
	MODPP_STATE_ENDIF,
	MODPP_STATE_FOREACH,
	MODPP_STATE_ENDFOR
} modpp_state_type_t;

typedef struct modpp_state {
	AUTOSTRING char* varname;
	boolean_t is_varset;

	modpp_state_type_t type;

	char* start;
	int lineno;
	
	int iter;

	list_node_t	node;
} modpp_state_t;

typedef struct modpp {
	int pp_lineno;
	char* pp_buf;
	char* pp_start;
	char* pp_end;
	size_t pp_size;

	FILE* pp_outf;

	modpp_state_t* pp_last;

	list_head_t pp_state;
} modpp_t;

int modpp_parse_var(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar);
int modpp_parse_dir_var(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar);
int modpp_parse_dir_novar(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar);

int modpp_subst(FILE* inf, FILE* outf);

#endif /* PREPROC_H_ */
