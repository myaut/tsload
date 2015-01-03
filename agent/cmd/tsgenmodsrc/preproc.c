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

#include <tsload/list.h>
#include <tsload/mempool.h>

#include <genmodsrc.h>
#include <preproc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

boolean_t modpp_trace = B_FALSE;

int modpp_subst_impl(modpp_t* pp);

/**
 * Modinfo preprocessor.
 *
 * It reads templates, substitutes macros and writes result to output file
 * (stdout or file on filesystem).
 *
 * Objects and structures:
 * 	* Preprocessor (modpp_t). Keeps entire template buffer and state stack
 * 	* State (modpp_state_t). Parsed token and related syntax construction. States \
 * 	  form a stack, because some of them support nesting.
 * 	* Current pointer. Pointer to a current character in template buffer. It is
 * 	  advanced by various parts of preprocessor.
 *
 * Core function of preprocessor is `modpp_subst_impl` which gets preprocessor object,
 * performs substitutions and writes result to output file. It may be called in two contexts:
 * when preprocessor works with entire template or when it meets a macro that defined there.
 *
 * When `modpp_subst_impl` meets symbol `@`, it calls `modpp_parse_directive`, that
 * tries to parse one of supported directives. That function reads variable name where
 * applicable, advances current pointer and writes chunk of text or create macro definitions
 * in modvars hashmap. If none directive matched, it tries to substitute if with one of
 * pre-defined module variables.
 *
 * To reduce number of calls, first text character pointer is saved into state->start variable.
 * When state changes (i.e. due to new directive), preprocessor writes (commits) chunk from
 * state->start to current pointer into output file if possible.
 *
 * BNF for parser:
 * ```
 * <opt-whitespace>		::= <whitespace> | <whitespace> <opt-whitespace> | ""
 *
 * <var-char> 			::= <alpha> | "_"
 * <var-name>			::= <var-char> | <var-char> <var-name>
 *
 * <dir-param-var>		::= <opt-whitespace> <var-name> <opt-whitespace> <EOL>
 * <dir-param-nothing>	::= <opt-whitespace> <EOL>
 *
 * <subst-expr>			::= "@" <var-name> "@"
 *
 * <block>				::= <text> | <ifdef-block> | <subst-expr>
 * <blocks>				::= <block> | <block> <blocks>
 *
 * <else-block>			::= "@else" <dir-param-nothing>
 * 							<blocks> <EOL>
 * <opt-else-block>		::= <else-block> | ""
 * <ifdef-block>		::= "@ifdef" <dir-param-var>
 * 							 <blocks> <EOL>
 *							 <opt-else-block>
 * 							"@endif" <dir-param-nothing>
 *
 * <define-block>		::= "@define" <dir-param-var>
 * 							<blocks>
 * 							"@enddef"
 * ```
 */

static const char* modpp_state_str(modpp_state_type_t type) {
	switch(type) {
	case MODPP_STATE_NORMAL:
		return "normal";
	case MODPP_STATE_VARNAME:
		return "varname";
	case MODPP_STATE_DEFINE:
		return "define";
	case MODPP_STATE_ENDDEF:
		return "enddef";
	case MODPP_STATE_IFDEF:
		return "ifdef";
	case MODPP_STATE_ELSE:
		return "else";
	case MODPP_STATE_ENDIF:
		return "endif";
	}

	return "???";
}

void modpp_do_trace(const char* action, modpp_t* pp, modpp_state_t* state, char* end) {
	fprintf(stderr, "%3d : %lx %6s %8s(%-16s) %d..%d\n",
			pp->pp_lineno, (unsigned long) pp,
			action, modpp_state_str(state->type),
			(state->varname) ? state->varname : "",
			state->start - pp->pp_start, end - pp->pp_start);
}

/**
 * create new state object and put it onto preprocessor stack
 *
 * @param pp current preprocessor
 * @param p pointer where state text starts
 * @param type state
 *
 * @note uses special death-object pp->pp_last to reduce number of allocations
 *
 * @return new object or NULL if it was failed to allocate
 */
modpp_state_t* modpp_create_state(modpp_t* pp, char* p, modpp_state_type_t type) {
	modpp_state_t* state = NULL;

	if(pp->pp_last == NULL) {
		state = mp_malloc(sizeof(modpp_state_t));
	}
	else {
		state = pp->pp_last;
		pp->pp_last = NULL;
	}

	aas_init(&state->varname);

	state->is_varset = B_FALSE;

	state->lineno = pp->pp_lineno;
	state->start = p;
	state->type = type;

	list_node_init(&state->node);
	list_add_tail(&state->node, &pp->pp_state);

	return state;
}

/**
 * remove state object and utilize allocated memory to preprocessor
 *
 * @param pp current preprocessor
 * @param state state object to be utilized
 */
void modpp_put_state(modpp_t* pp, modpp_state_t* state) {
	if(modpp_trace) {
		modpp_do_trace("PUT", pp, state, state->start);
	}

	aas_free(&state->varname);
	list_del(&state->node);

	if(pp->pp_last != NULL) {
		mp_free(state);
	}
	else {
		pp->pp_last = state;
	}

	if(modpp_trace) {
		modpp_state_t* state = list_last_entry(modpp_state_t, &pp->pp_state, node);
		modpp_do_trace("<-", pp, state, state->start);
	}
}

/**
 * pick top state from preprocessor stack and set start pointer to p
 *
 * @param pp current preprocessor
 * @param p pointer to current character
 */
void modpp_reset_state(modpp_t* pp, char* p) {
	modpp_state_t* state = list_last_entry(modpp_state_t, &pp->pp_state, node);

	if(modpp_trace) {
		modpp_do_trace("RESET", pp, state, p);
	}

	state->start = p;
}

/**
 * completely destroy state object
 *
 * @param obj state object
 */
void modpp_destroy_state(void* obj) {
	modpp_state_t* state = (modpp_state_t*) obj;

	aas_free(&state->varname);
	list_del(&state->node);

	mp_free(state);
}

/**
 * create new preprocessor
 *
 * @param lineno starting line number (for define - lineno of directive)
 * @param p pointer to buffer
 * @param size number of useful characters in buffer
 * @param outf output file for preprocessor
 *
 * @return new preprocessor
 */
modpp_t* modpp_create_pp(int lineno, char* p, size_t size, FILE* outf) {
	modpp_t* pp = mp_malloc(sizeof(modpp_t));
	modpp_state_t* state = NULL;

	pp->pp_lineno = lineno;
	pp->pp_size = size;
	pp->pp_buf = NULL;
	pp->pp_start = p;
	pp->pp_end = p + size;

	pp->pp_last = NULL;
	pp->pp_outf = outf;

	list_head_init(&pp->pp_state, "modpp_state");

	state = modpp_create_state(pp, p, MODPP_STATE_NORMAL);
	state->start = p;

	return pp;
}

/**
 * destroy preprocessor: destroy state stack, free buffers and pp object
 *
 * @return MODPP_UNCLOSED_DIRECTIVE if there are more than one directive on stack \
 * 		   or MODPP_OK
 */
int modpp_destroy_pp(modpp_t* pp) {
	modpp_state_t* state = NULL;
	modpp_state_t* next = NULL;
	int ret = MODPP_OK;

	if(!list_empty(&pp->pp_state) && !list_is_singular(&pp->pp_state)) {
		state = list_last_entry(modpp_state_t, &pp->pp_state, node);

		fprintf(stderr, "Parse error: unclosed directive %s at line %d\n",
				modpp_state_str(state->type), state->lineno);
		ret = MODPP_UNCLOSED_DIRECTIVE;
	}

	list_for_each_entry_safe(modpp_state_t, state, next, &pp->pp_state, node) {
		modpp_destroy_state(state);
	}

	if(pp->pp_last) {
		mp_free(pp->pp_last);
	}

	if(pp->pp_buf) {
		mp_free(pp->pp_buf);
	}

	mp_free(pp);

	return ret;
}

/**
 * write variable to output file
 *
 * @param pp preprocessor
 * @param varstate state related to variable, should be MODPP_STATE_VARNAME
 * @param p current walk pointer, not used
 */
static int modpp_commit_var(modpp_t* pp, modpp_state_t* varstate, char* p) {
	modvar_t* var = modvar_get(varstate->varname);

	if(var == NULL) {
		fprintf(stderr, "Parse error: unknown variable '%s' at line %d\n",
				varstate->varname, varstate->lineno);

		return MODPP_VAR_NOT_EXISTS;
	}

	if(var->value) {
		fputs(var->value, pp->pp_outf);
	}
	else if(var->valgen) {
		var->valgen(pp->pp_outf, var->valgen_arg);
	}
	else {
		fprintf(stderr, "Parse error: unknown variable '%s': "
				"no value or generator at line %d\n",
				varstate->varname, varstate->lineno);

		return MODPP_VAR_NOT_EXISTS;
	}

	if(modpp_trace) {
		fflush(pp->pp_outf);
	}

	return MODPP_OK;
}

/**
 * write text to output file
 *
 * @param pp preprocessor
 * @param state state related to variable, should be MODPP_STATE_NORMAL or \
 * 	MODPP_STATE_IFDEF
 * @param end current walk pointer
 */
static int modpp_commit_text(modpp_t* pp, modpp_state_t* state, char* end) {
	int ret = MODPP_OK;

	if(end <= state->start) {
		return ret;
	}

	fwrite(state->start, 1, end - state->start, pp->pp_outf);
	if(modpp_trace) {
		modpp_do_trace("TEXT", pp, state, end);
		fflush(pp->pp_outf);
	}

	return ret;
}

void modpp_define_generate(FILE* outf, void* arg) {
	modpp_t* pp = (modpp_t*) arg;
	modpp_state_t* state = list_first_entry(modpp_state_t, &pp->pp_state, node);

	pp->pp_outf = outf;
	modpp_subst_impl(pp);

	/* Rewind normal state to begin */
	state->start = pp->pp_start;
}

/**
 * create macro variable and put it into modvars hashmap
 *
 * @param pp preprocessor
 * @param def state related to variable, should be MODPP_STATE_DEFINE
 * @param end current walk pointer
 */
static int modpp_commit_define(modpp_t* pp, modpp_state_t* def, char* end) {
	modpp_t* defpp;
	modvar_t* var = NULL;

	if(modpp_trace) {
		modpp_do_trace("DEF", pp, def, end);
	}

	/* Create new preprocessor for define */
	defpp = modpp_create_pp(def->lineno + 1, def->start, end - def->start, NULL);

	var = modvar_set_gen(modvar_create_dn(def->varname), modpp_define_generate,
						 modpp_destroy_pp, defpp);

	if(var == NULL) {
		fprintf(stderr, "Cannot create define '%s' - may be already exists",
				def->varname);
		return MODPP_DEFINE_ERROR;
	}

	return MODPP_OK;
}

/**
 * Returns B_TRUE if there are \@ifdef directives on stack and
 * we are on the path where all variables are defined
 */
static boolean_t modpp_is_defined(modpp_t* pp) {
	modpp_state_t* state;
	boolean_t in_ifdef;

	list_for_each_entry(modpp_state_t, state, &pp->pp_state, node) {
		if(state->type == MODPP_STATE_IFDEF) {
			in_ifdef = B_TRUE;
			if(state->is_varset)
				return B_FALSE;
		}
		else if(state->type == MODPP_STATE_ELSE) {
			in_ifdef = B_TRUE;
			if(!state->is_varset)
				return B_FALSE;
		}
	}

	return in_ifdef;
}

static int modpp_commit_ifdef(modpp_t* pp, modpp_state_t* state, char* end) {
	if(!modpp_is_defined(pp)) {
		return MODPP_OK;
	}

	return modpp_commit_text(pp, state, end);
}

#define MODPP_PARSE_DIR(name, type, parser, endchar)		\
	if(strncmp(dir_name, name, sizeof(name) - 1) == 0) {	\
		new_state = modpp_create_state(pp, p, type);		\
		p += sizeof(name);									\
		ret = parser(pp, new_state, &p, endchar);			\
	}

#define MODPP_PARSE_VAR(type, parser, endchar) 				\
	{														\
		new_state = modpp_create_state(pp, p, type);		\
		++p;												\
		ret = parser(pp, new_state, &p, endchar);			\
	}


/**
 * main parse function
 *
 * Tries to read directive at pointer and creates `new_state` object for it.
 * Checks current state and `new_state` and advance FSM or returns an error
 *
 * If possible, commit chunks of text and variables.
 *
 * @param pp preprocessor
 * @param ptr pointer to current walk pointer
 */
int modpp_parse_directive(modpp_t* pp, char** ptr) {
	modpp_state_t* state = list_last_entry(modpp_state_t, &pp->pp_state, node);
	modpp_state_t* new_state = NULL;
	int ret = MODPP_OK;
	char* p = *ptr;
	char* end = p;
	char* dir_name = p + 1;

	/* Parse current directive and write it to a new_state */
	 	 MODPP_PARSE_DIR("ifdef",  MODPP_STATE_IFDEF,   modpp_parse_dir_var, '\0')
	else MODPP_PARSE_DIR("define", MODPP_STATE_DEFINE,  modpp_parse_dir_var, '\0')
	else MODPP_PARSE_DIR("enddef", MODPP_STATE_ENDDEF,  modpp_parse_dir_novar, '\0')
	else MODPP_PARSE_DIR("else",   MODPP_STATE_ELSE,    modpp_parse_dir_novar, '\0')
	else MODPP_PARSE_DIR("endif",  MODPP_STATE_ENDIF,   modpp_parse_dir_novar, '\0')
	else MODPP_PARSE_VAR(		   MODPP_STATE_VARNAME, modpp_parse_var, '@');

	if(ret != MODPP_OK)
		return ret;


	/* Pointers are set as follows:
	 * ....@directive....\n....
	 *    1 2             3
	 *  1 - end
	 *  2 - dir_name
	 *  3 - p
	 */

	*ptr = p;
	new_state->start = p + 1;

	if(modpp_trace) {
		modpp_do_trace("STATE", pp, state, end);
		modpp_do_trace("->", pp, new_state, p);
	}

	/* Advance state machine */
	switch(state->type) {
	case MODPP_STATE_DEFINE:
		if(new_state->type == MODPP_STATE_ENDDEF) {
			ret = modpp_commit_define(pp, state, end);

			modpp_put_state(pp, state);
			modpp_put_state(pp, new_state);

			/* Remove unnecessary newlines after define */
			if(*p == '\n')
				++p;

			modpp_reset_state(pp, p + 1);

			return ret;
		}
		else if(new_state->type == MODPP_STATE_DEFINE) {
			fprintf(stderr, "Parse error: nested 'define' at line %d\n", state->lineno);
			return MODPP_INVALID_DIRECTIVE;
		}
		else {
			/* Ignore other directives in define, will re-parse them later */
			modpp_put_state(pp, new_state);

			return MODPP_OK;
		}
		break;
	case MODPP_STATE_IFDEF:
		if(new_state->type == MODPP_STATE_ELSE) {
			modpp_put_state(pp, new_state);

			/* We are moving onto ifdef->else path, commit last chunks of
			 * ifdef if its variable defined, than set state = new_state */
			modpp_commit_ifdef(pp, state, end);

			state->start = p + 1;
			state->type = MODPP_STATE_ELSE;

			return MODPP_OK;
		}
		/* FALLTHROUGH */
	case MODPP_STATE_ELSE:
		if(new_state->type == MODPP_STATE_ENDIF) {
			/* Remove unnecessary newlines before endif */
			if(*(end - 1) == '\n')
				--end;
		}

		/* Regardless of new_state, try to commit last chunk */
		modpp_commit_ifdef(pp, state, end);

		if(new_state->type == MODPP_STATE_ENDIF) {
			/* Remove endif and corresponding ifdef/else from stack */
			modpp_put_state(pp, state);
			modpp_put_state(pp, new_state);
			modpp_reset_state(pp, p + 1);

			return MODPP_OK;
		}
		else if(new_state->type == MODPP_STATE_VARNAME) {
			/* If ifdef variable is defined, commit variable inside ifdef text */
			if(modpp_is_defined(pp))
				ret = modpp_commit_var(pp, new_state, p + 1);
		}
		else if(new_state->type != MODPP_STATE_IFDEF) {
			/* define and enddef are not supported inside ifdef */
			fprintf(stderr, "Parse error: unexpected '%s' at line %d\n",
					modpp_state_str(state->type), state->lineno);
			return MODPP_INVALID_DIRECTIVE;
		}
		break;
	case MODPP_STATE_NORMAL:
		/* normal text block, do what you should do */
		modpp_commit_text(pp, state, end);
		if(new_state->type == MODPP_STATE_VARNAME) {
			ret = modpp_commit_var(pp, new_state, p + 1);
		}
		break;
	}

	if(new_state->type == MODPP_STATE_VARNAME) {
		/* remove varname from stack */
		modpp_put_state(pp, new_state);
		modpp_reset_state(pp, p + 1);
	}

	return ret;
}

static int modpp_unexpected_char_error(modpp_t* pp, modpp_state_t* state, char* p) {
	fprintf(stderr, "Parse error: unexpected character '%c' in directive '%s' at line %d\n",
					*p, modpp_state_str(state->type), state->lineno);
	return MODPP_INVALID_DIRECTIVE;
}

/**
 * parse a variable inside a directive ifdef/define or parse varname
 *
 * @param pp preprocessor
 * @param state state related to directive/varname
 * @param ptr pointer to current pointer
 * @param endchar character that flags state end, newline for directives, `@` for variables
 */
int modpp_parse_var(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar) {
	char* p = *ptr;
	char* varstart = NULL;
	char* end = NULL;

	while(*p != endchar) {
		if(*p == ' ' || *p == '\t') {
			++p;
			continue;
		}
		else if(varstart == NULL) {
			varstart = p;
		}

		end = p;

		if(!isalpha(*p) && *p != '_') {
			return modpp_unexpected_char_error(pp, state, p);
		}

		++p;
	}

	if(varstart == NULL || end == NULL) {
		fprintf(stderr, "Parse error: missing variable in directive '%s' at line %d\n",
				modpp_state_str(state->type), state->lineno);
		return MODPP_MISSING_VAR;
	}

	aas_copy_n(&state->varname, varstart, end - varstart);
	state->is_varset = modvar_is_set(state->varname);

	*ptr = p;

	return MODPP_OK;
}

/**
 * parse directive with variable - ignore whitespace and parse variable
 */
int modpp_parse_dir_var(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar) {
	char* varstart = NULL;
	char* p = *ptr;

	/* Ignore whitespace */
	while(*p == ' ' || *p == '\t')
		++p;

	return modpp_parse_var(pp, state, ptr, '\n');
}

/**
 * parse directive without variable - ignore whitespace and ensure there are
 * no other characters
 */
int modpp_parse_dir_novar(modpp_t* pp, modpp_state_t* state, char** ptr, char endchar) {
	char* p = *ptr;

	while(*p == ' ' || *p == '\t')
		++p;

	if(*p != '\n') {
		return modpp_unexpected_char_error(pp, state, p);
	}

	*ptr = p;

	return MODPP_OK;
}

/**
 * do the substitutions
 */
int modpp_subst_impl(modpp_t* pp) {
	int ret = MODPP_OK;
	char* p = pp->pp_start;
	modpp_state_t* state = NULL;

	while(p != pp->pp_end) {
		if(*p == '@') {
			ret = modpp_parse_directive(pp, &p);
		}

		if(*p == '\n') {
			++pp->pp_lineno;
		}

		if(ret != MODPP_OK)
			break;

		++p;
	}

	if(list_is_singular(&pp->pp_state)) {
		state = list_last_entry(modpp_state_t, &pp->pp_state, node);
		if(state->type == MODPP_STATE_NORMAL) {
			modpp_commit_text(pp, state, p);
		}
	}

	return ret;
}

static int modpp_read_inf(FILE* inf, char** pbuffer, size_t* psize) {
	size_t size, result;
	char* buffer;

	/* Read entire file into buffer */
	fseek(inf, 0, SEEK_END);
	size = ftell(inf);
	fseek(inf, 0, SEEK_SET);

	buffer = mp_malloc(size  + 1);
	result = fread(buffer, 1, size, inf);
	buffer[size] = '\0';

	if(result < size) {
		fprintf(stderr, "Cannot read input file: errno = %d\n", errno);
		mp_free(buffer);
		return MODPP_READ_ERROR;
	}

	*pbuffer = buffer;
	*psize = size;

	return MODPP_OK;
}

int modpp_subst(FILE* inf, FILE* outf) {
	modpp_t* pp = NULL;
	modpp_state_t* state = NULL;
	char* buffer;
	size_t size;

	int ret = MODPP_OK;

	char* p = NULL;

	/* Create modpp_t and read entire file into memory */
	ret = modpp_read_inf(inf, &buffer, &size);
	if(ret != MODPP_OK)
		goto end;

	pp = modpp_create_pp(1, buffer, size, outf);
	pp->pp_buf = buffer;

	ret = modpp_subst_impl(pp);

end:
	ret = modpp_destroy_pp(pp);

	return ret;
}
