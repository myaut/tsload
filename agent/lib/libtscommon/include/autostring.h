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

#ifndef AUTOSTRING_H_
#define AUTOSTRING_H_

#include <defs.h>

#include <stdlib.h>
#include <stdarg.h>

/**
 * @module Automatically allocated strings.
 *
 * Used instead of statically-allocated strings as read-only buffers
 * (after allocation, string should be freed and allocated again)
 *
 * Since arguments passed to AAS may be mutable (but should not be),
 * it uses "n" functions to protect from possible buffer overflow.
 */

/**
 * AAS errors
 *
 * @value AAS_BUF_NOT_EMPTY string was already allocated
 * @value AAS_ALLOCATION_ERROR failed to allocate new buffer
 * @value AAS_INVALID_ARGUMENT one of required argument is invalid (probably NULL)
 */
#define AAS_BUF_NOT_EMPTY		((size_t) -1)
#define AAS_ALLOCATION_ERROR	((size_t) -2)
#define AAS_INVALID_ARGUMENT	((size_t) -3)

#define AAS_MAX_ERROR			16

/**
 * Checks if error was occured in aas function
 */
#define AAS_IS_OK(ret)			((ret + AAS_MAX_ERROR) >= AAS_MAX_ERROR)

#define AAS_DYNAMIC				'D'
#define AAS_STATIC				'S'
#define AAS_STATIC_PREFIX		"S"

/* Detecting statically allocated strings:
 *   $ find . -type f -name \*.h |
 *   	grep -v build |
 *   	xargs egrep 'char\s+[a-zA-Z_0-9]+\s*\[.*\]\s*;'
 */

LIBEXPORT char* aas_allocate(size_t count);

LIBEXPORT size_t aas_vprintf(char** aas, const char* format, va_list va);
STATIC_INLINE size_t aas_printf(char** aas, const char* format, ...) {
	va_list va;
	size_t ret;

	va_start(va, format);
	ret = aas_vprintf(aas, format, va);
	va_end(va);

	return ret;
}

LIBEXPORT size_t aas_merge(char** aas, const char* str, ...);

LIBEXPORT size_t aas_copy_n(char** aas, const char* str, size_t count);
LIBEXPORT size_t aas_copy(char** aas, const char* str);

LIBEXPORT void aas_set_impl(char** aas, const char* str);

/**
 * Set auto-allocated string to compile-time constant value
 *
 * @param aas pointer to auto-allocated string
 * @param str constant string literal
 */
#define AAS_CONST_STR(str)		((AAS_STATIC_PREFIX str) + 1)
#define aas_set(aas, str)		aas_set_impl(aas, AAS_STATIC_PREFIX str)

static char** aas_init(char** aas) {
	*aas = NULL;
	return aas;
}
LIBEXPORT void aas_free(char** aas);

#endif /* AUTOSTRING_H_ */
