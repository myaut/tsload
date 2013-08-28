/*
 * threads.h
 *
 *  Created on: 20.12.2012
 *      Author: myaut
 */

#ifndef PLAT_POSIX_THREADS_H_
#define PLAT_POSIX_THREADS_H_

#include <pthread.h>

typedef struct {
	pthread_cond_t  tcv_cv;
} plat_thread_cv_t;

typedef struct {
	pthread_mutex_t	tm_mutex;
} plat_thread_mutex_t;

typedef struct {
	pthread_rwlock_t	tl_rwlock;
} plat_thread_rwlock_t;

typedef struct {
	pthread_attr_t 	t_attr;
	pthread_t		t_thread;
} plat_thread_t;

typedef struct {
	pthread_key_t 	tk_key;
} plat_thread_key_t;

#define PLAT_THREAD_EVENT_INITIALIZER 					\
	{ SM_INIT(.te_mutex, PTHREAD_MUTEX_INITIALIZER),	\
	  SM_INIT(.te_cv, PTHREAD_COND_INITIALIZER)  }

#define PLAT_THREAD_MUTEX_INITIALIZER 					\
	{ SM_INIT(.tm_mutex, PTHREAD_MUTEX_INITIALIZER) }

#define PLAT_THREAD_KEY_INITIALIZER 					\
	{ SM_INIT(.tk_key, 0) }

typedef void* thread_result_t;
typedef void* thread_arg_t;

#define PLAT_THREAD_FINISH(arg, code) 		\
	pthread_exit(&(code));					\
	return arg;

#endif /* PLAT_POSIX_THREADS_H_ */
