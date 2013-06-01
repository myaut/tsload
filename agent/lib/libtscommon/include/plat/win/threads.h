/*
 * threads.h
 *
 *  Created on: 20.12.2012
 *      Author: myaut
 */

#ifndef PLAT_WIN_THREADS_H_
#define PLAT_WIN_THREADS_H_

#include <windows.h>

typedef struct {
	CRITICAL_SECTION te_crit_section;
	CONDITION_VARIABLE te_cond_var;
} plat_thread_event_t;

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
