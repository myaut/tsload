
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



#ifndef TUNEIT_H_
#define TUNEIT_H_

#include <tsload/defs.h>

#include <tsload/list.h>

#include <stdlib.h>


/**
 * Tuneable option
 *
 * @member	buf		buffer used to keep option string. Should be freed by libc free()
 * @member  name	pointer to _name_ inside buffer
 * @member  value   pointer to _value_ inside buffer
 * @member  bval	boolean +/- value
 * @member  node	linked list node
 */
typedef struct {
	char*	buf;

	char*	name;
	char* 	value;
	boolean_t bval;

	list_node_t		node;
} tuneit_opt_t;

#define	TUNEIT_OK				0
#define	TUNEIT_NOTSET			1
#define TUNEIT_INVALID			-1
#define TUNEIT_UNKNOWN			-2

LIBEXPORT int tuneit_add_option(char* optstr);

LIBEXPORT int tuneit_set_int_impl(const char* name, size_t sz, void* ptr);
#define tuneit_set_int(type, var)	tuneit_set_int_impl(#var, sizeof(type), &var)

LIBEXPORT int tuneit_set_bool_impl(const char* name, boolean_t* ptr);
#define tuneit_set_bool(var)	tuneit_set_bool_impl(#var, &var)

LIBEXPORT int tuneit_set_string_impl(const char* name, char* ptr, size_t length);
#define tuneit_set_string(var, length) tuneit_set_string_impl(#var, var, length)

LIBEXPORT int tuneit_finalize(void);

#endif /* TUNEIT_H_ */


