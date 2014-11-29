
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#ifndef PLAT_WIN_THREADS_H_
#define PLAT_WIN_THREADS_H_

#include <tsload/defs.h>

#include <windows.h>


typedef struct {
	CONDITION_VARIABLE tcv_cond_var;
} plat_thread_cv_t;

typedef struct {
	CRITICAL_SECTION tm_crit_section;
} plat_thread_mutex_t;

#define TL_MODE_FREE			0
#define TL_MODE_EXCLUSIVE		1
#define TL_MODE_SHARED			2

typedef struct {
	SRWLOCK tl_slim_rwlock;
	DWORD tl_mode_key;
} plat_thread_rwlock_t;

typedef struct {
	HANDLE t_handle;
} plat_thread_t;

typedef struct {
	DWORD tk_tls;
} plat_thread_key_t;

#define PLAT_THREAD_EVENT_INITIALIZER 	\
	{ 0   }

#define PLAT_THREAD_MUTEX_INITIALIZER 	\
	{ 0 }

#define PLAT_THREAD_KEY_INITIALIZER 	\
	{ 0 }

typedef DWORD  thread_result_t;
typedef LPVOID thread_arg_t;

#define PLAT_THREAD_FINISH(arg, code) 	\
	return (code);

#endif /* PLAT_WIN_THREADS_H_ */

