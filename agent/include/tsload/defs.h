
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef DEFS_H_
#define DEFS_H_

#include <tsload/defs.h>

#include <stddef.h>
#include <stdint.h>

#include <tsload/genconfig.h>


#define TO_BOOLEAN(x)	!!(x)

#ifndef HAVE_BOOLEAN_T
typedef enum { B_FALSE, B_TRUE } boolean_t;
#else
/* On Solaris boolean defined in <sys/types.h> */
#include <sys/types.h>
#endif

#define SZ_KB	(1024ll)
#define SZ_MB 	(1024ll * SZ_KB)
#define SZ_GB	(1024ll * SZ_MB)
#define SZ_TB	(1024ll * SZ_GB)

/*All time periods measured in ns*/
#define T_NS 		1ll
#define T_US		(1000ll * T_NS)
#define T_MS		(1000ll * T_US)
#define T_SEC		(1000ll * T_MS)

/* Platform API */
#define PLATAPI
#define PLATAPIDECL(...)

/* Checkformat provides warnings for incorrect printf format */
#ifdef __GNUC__
#	define CHECKFORMAT(func, arg1, arg2) __attribute__ ((format (func, arg1, arg2)))
#else
#	define CHECKFORMAT(func, arg1, arg2)
#endif

/* Initialize structure member
 * VisualC doesn't support designated initializers (C99), so it cannot use form .member = value
 *
 * Checked by CheckDesignatedInitializers */
#ifdef HAVE_DESIGNATED_INITIALIZERS
#	define SM_INIT(member, value)   member = value
#else
#	define SM_INIT(member, value)	value
#endif

#ifdef _MSC_VER
#	define STATIC_INLINE static __inline
#else
#	define STATIC_INLINE static inline
#endif

#define container_of(ptr, type, member) ((type*) ((char*)ptr - offsetof(type, member)))

#define uninitialized_var(x) x = x

/* likely and unlikely conditional optimizations */
#ifdef __GNUC__
#  define likely(x)		(__builtin_expect(!!(x), 1))
#  define unlikely(x)	(__builtin_expect(!!(x), 0))
#else
#  define likely(x)		x
#  define unlikely(x)	x
# endif

/* On Windows we should define special markings for exported functions
 * and variables from shared libraries.
 *
 * Use LIBEXPORT in header for functions
 * Use LIBIMPORT for shared data in executable code*/
#ifdef _MSC_VER
#define LIBEXPORT			__declspec(dllexport)
#define LIBIMPORT			__declspec(dllimport)
#else
#define LIBEXPORT
#define LIBIMPORT			extern
#endif

/* This macro guarantees correct linking of C++ code with C variables
 */
#ifdef __cplusplus
#define LIBVARIABLE 		extern "C"
#else
#define LIBVARIABLE 		extern
#endif

/* Some functions are needed by unit tests
 * FIXME: Make this configurable
 */
#define TESTEXPORT			LIBEXPORT

/* For GNU C we need to use attributes with packed structures
 * For VC++ it is defined using #pragma pack(...). GNU C supports for compability */
#ifdef __GNUC__
#  define PACKED_STRUCT		__attribute__((packed))
#else
#  define PACKED_STRUCT
# endif

/* Define min and max macroses.
 * VS has it's own min/max implementation, so use it */
#ifndef HAVE_DECL_MIN
#	define min(a, b) ((a) < (b)? (a) : (b))
#endif

#ifndef HAVE_DECL_MAX
#	define max(a, b) ((a) > (b)? (a) : (b))
#endif

#if defined(HAVE_DECL_MIN) || defined(HAVE_DECL_MAX)
#	include <stdlib.h>
#endif

#ifndef HAVE_DECL_ROUND
#include <math.h>
STATIC_INLINE double round(double val)
{
    return floor(val + 0.5);
}
#endif

#ifndef HAVE_INTTYPES_H
#	if defined(_MSC_VER)
#		include <tsload/msc_inttypes.h>
#	else
# 		error "<inttypes.h> is missing"
#	endif
#else
# 	include <inttypes.h>
#endif

/* Format specifiers for size_t and ssize_t */
#if defined(_MSC_VER)
#define PRIsz		"Iu"
#define PRIssz		"Id"
#else
#define PRIsz		"zu"
#define PRIssz		"zd"
#endif

#ifndef HAVE_DECL_VA_COPY
#ifdef _MSC_VER
#define va_copy(dst, src) (dst = src)
#else
#	error "Don't have va_copy() on your platform"
#endif
#endif

/* Hint to TSDOC if function is documentable or should be strongly ignored */
#define TSDOC_HIDDEN
#define TSDOC_FORCE

/* MSVC CRT doesn't have setenv, but it can be replaced with _putenv_s() call */
#ifdef PLAT_WIN
#define setenv(var, val, overwrite)     _putenv_s(var, val)
#endif

/**
 * Hint that string is automatically allocated
 */
#define AUTOSTRING

#endif /* DEFS_H_ */

