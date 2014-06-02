
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



#ifndef FIELD_H_
#define FIELD_H_

#include <defs.h>

#include <stdlib.h>

/**
 * @module Field access
 *
 * Some TSLoad subsystems (libtsfile and workload parameters) are implementing a kind of reflection:
 * offset of field is saved in static structure and then field is filled in dynamically.
 *
 * However some architectures (i.e. SPARC) do not allow unaligned reads, so it breaks
 * field accesses and cause bus errors. */

#ifdef HAVE_UNALIGNED_MEMACCESS

/**
 * Declare helpers for accessing fields */
#define DECLARE_FIELD_FUNCTIONS(type)

/**
 * Special byte access helper */
#define DECLARE_FIELD_FUNCTION_BYTE(type)

/**
 * Read value from structure */
#define	FIELD_GET_VALUE(type, value)				*((type*) value)

/**
 * Write value to structure */
#define	FIELD_PUT_VALUE(type, value, new_value)		*((type*) value) = (type) (new_value)


#else

/* TODO: For gcc implement inlined construction ({...}) */

#define FIELD_IS_ALIGNED(value, type)		((((unsigned long) value) & (-sizeof(type))) == 0)

#define DECLARE_FIELD_FUNCTIONS(type)								\
	STATIC_INLINE type field_get_ ## type(char* value)	{			\
		type tmp;													\
		if(FIELD_IS_ALIGNED(value, type))							\
			return *((type*) value);								\
		memcpy(&tmp, value, sizeof(type));							\
		return tmp;													\
	}																\
	STATIC_INLINE void field_put_ ## type							\
				(char* value, type new_value)	{					\
		if(FIELD_IS_ALIGNED(value, type))							\
			*((type*) value) = new_value;							\
		else														\
			memcpy(value, &new_value, sizeof(type));				\
	}

/* Byte accesses are always aligned, so provide special implementation for it */
#define DECLARE_FIELD_FUNCTION_BYTE(type)							\
	STATIC_INLINE type field_get_ ## type(char* value) {			\
		return *((type*) value);									\
	}																\
	STATIC_INLINE void field_put_ ## type							\
			(char* value, type new_value) {							\
		*((type*) value) = new_value;								\
	}

#define	FIELD_GET_VALUE(type, value)			field_get_ ## type(value)
#define	FIELD_PUT_VALUE(type, value, new_value)	field_put_ ## type(value, new_value)
#endif


#endif /* FIELD_H_ */

