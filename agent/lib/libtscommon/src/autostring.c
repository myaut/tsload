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

#include <mempool.h>
#include <autostring.h>

#include <stdio.h>
#include <string.h>

/**
 * Allocate new auto-allocated string.
 * Use only with pair of aas_free
 *
 * @param count number of characters without null-terminator
 *
 * @return NULL or pointer to allocated buffer
 */
char* aas_allocate(size_t count) {
	char* buffer = mp_malloc(count + 2);

	if(buffer == NULL)
		return NULL;

	*buffer = AAS_DYNAMIC;

	return buffer + 1;
}

void aas_set_impl(char** aas, const char* str) {
	*aas = str + 1;
}

/**
 * Formatted print for auto-allocated strings
 *
 * @param aas pointer to auto-allocated string
 * @param format printf's format string
 *
 * @return number of printed characters or AAS error
 */
size_t aas_printf(char** aas, const char* format, ...) {
	/* Implementation taken from
	 * http://stackoverflow.com/questions/4899221/substitute-or-workaround-for-asprintf-on-aix */
	va_list va;
	size_t count;

	if(format == NULL)
		return AAS_INVALID_ARGUMENT;
	if(*aas != NULL)
		return AAS_BUF_NOT_EMPTY;

	va_start(va, format);
	count = vsnprintf(NULL, 0, format, va);
	va_end(va);

	if (count >= 0)
	{
		char* buffer = aas_allocate(count);
		if (buffer == NULL)
			return AAS_ALLOCATION_ERROR;

		va_start(va, format);
		count = vsnprintf(buffer, count + 1, format, va);
		va_end(va);

		if (count < 0)
		{
			free(buffer);
			return count;
		}

		*aas = buffer;
	}

	return count;
}

/**
 * Merge several strings into one and allocate it
 *
 * @note last string should be set to NULL
 *
 * @param aas pointer to auto-allocated string
 * @param str first piece of strings
 *
 * @return number of printed characters or AAS error
 */
size_t aas_merge(char** aas, const char* str, ...) {
	va_list va;
	size_t count = 0;
	const char* tmp = str;

	char* buffer;

	if(str == NULL)
		return AAS_INVALID_ARGUMENT;
	if(*aas != NULL)
		return AAS_BUF_NOT_EMPTY;

	/* Calculate length of destination string */
	va_start(va, str);
	do {
		count += strlen(str);

		str = va_arg(va, const char*);
	} while(str != NULL);
	va_end(va);

	/* Allocate buffer */
	buffer = aas_allocate(count);
	if (buffer == NULL)
		return AAS_ALLOCATION_ERROR;

	/* Now copy strings
	 * FIXME: strncat is slower, should keep pointer to the NULL-terminator */
	strncpy(buffer, tmp, count + 1);

	va_start(va, str);
	str = va_arg(va, const char*);
	while(str != NULL) {
		strncat(buffer, str, count + 1);

		str = va_arg(va, const char*);
	}
	va_end(va);

	*aas = buffer;

	return count;
}

/**
 * Copy string to auto-allocated string
 *
 * @param aas pointer to auto-allocated string
 * @param str source string
 *
 * @return number of printed characters or AAS error
 */
size_t aas_copy(char** aas, const char* str) {
	size_t count;

	char* buffer;

	if(str == NULL)
		return AAS_INVALID_ARGUMENT;
	count = strlen(str);

	buffer = aas_allocate(count);
	if (buffer == NULL)
		return AAS_ALLOCATION_ERROR;

	strncpy(buffer, str, count + 1);

	*aas = buffer;

	return count;
}

/**
 * Frees auto-allocated string (if it was dynamically allocated)
 */
void aas_free(char** aas) {
	char* buffer;

	if(*aas != NULL) {
		buffer = *aas - 1;

		if(*buffer == AAS_DYNAMIC) {
			mp_free(buffer);
		}

		*aas = NULL;
	}
}
