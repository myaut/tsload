/*
 * threads.h
 *
 *  Created on: 10.11.2012
 *      Author: myaut
 */

#ifndef THREADS_H_
#define THREADS_H_

#include <stdint.h>

#include <plat/schedutil.h>
#include <plat/threads.h>

#include <defs.h>
#include <tstime.h>
#include <atomic.h>

#ifdef PLAT_SOLARIS
/* Solaris has it's own definitions of mutexes,
 * so hide our implementation with ts_ prefix */
#define mutex_init			ts_mutex_init
#define mutex_destroy		ts_mutex_destroy
#define	mutex_lock			ts_mutex_lock
#define	mutex_try_lock		ts_mutex_try_lock
#define	mutex_unlock		ts_mutex_unlock

#define cv_init				ts_cv_init
#define cv_wait				ts_cv_wait
#define cv_notify_one		ts_cv_notify_one
#define cv_notify_all		ts_cv_notify_all
#define cv_destroy			ts_cv_destroy
#endif

#define TEVENTNAMELEN	32
#define TCVNAMELEN		32
#define TMUTEXNAMELEN	32
#define TKEYNAMELEN 	32
#define TRWLOCKNAMELEN	32
#define TNAMELEN		48

#define THASHSHIFT		4
#define THASHSIZE		(1 << THASHSHIFT)
#define THASHMASK		(THASHSIZE - 1)

/**
 * @module Threads and syncronization primitives
 *
 * Provides cross-platform interface for threads handling and synchronization primitives.
 *
 * Example of thread's function:
 * ```
 * thread_result_t worker_thread(thread_arg_t arg) {
 *    THREAD_ENTRY(arg, workload_t, wl);
 *
 *    ...
 *    if(cond)
 *    	 THREAD_EXIT(1);
 *    ...
 *
 * THREAD_END:
 * 	  smth_destroy(...);
 * 	  THREAD_FINISH(arg);
 * }
 * ```
 *
 * You may pass an context variable into thread in t_init(), like workload_t
 * in this example. THREAD_ENTRY macro declares it and special variable 'thread':
 * ```
 *    thread_t* thread;
 * 	  workload_t* wl;
 * ```
 *
 * @note On Solaris mutex_* and cv_* function names are reserved by libc, so threads library \
 * 	     prefixes it's names with ts.
 */

/**
 * THREAD_ENTRY macro should be inserted into beginning of thread function
 * declares thread_t* thread and argtype_t* arg_name (private argument passed to t_init)
 * */
#define THREAD_ENTRY(arg, arg_type, arg_name) 			\
	thread_t* thread = t_post_init((thread_t*) arg);	\
	arg_type* arg_name = (arg_type*) thread->t_arg

/**
 * THREAD_END and THREAD_FINISH are used as thread's epilogue:
 *
 * THREAD_END:
 *     De-initialization code goes here
 * 	   THREAD_FINISH(arg);
 * }
 * */
#define THREAD_END 		_l_thread_exit
#define THREAD_FINISH(arg)    						\
	atomic_set(&thread->t_state_atomic, TS_DEAD);	\
	t_exit(thread);									\
	PLAT_THREAD_FINISH(arg, thread->t_ret_code)

/**
 * THREAD_EXIT - prematurely exit from thread (works only in main function of thread)
 * */
#define THREAD_EXIT(code)			\
	thread->t_ret_code = code;		\
	goto _l_thread_exit

/**
 * Thread states:
 *
 * ```
 *       |
 *     t_init              +----------------------------------------------+
 *       |                 |                                              |
 * TS_INITIALIZED --> TS_RUNNABLE --+--event_wait--> TS_WAITING ----------+
 *                         |        \                                     |
 *                       t_exit      \--mutex_lock--> TS_LOCKED ----------+
 *                         |
 *                         v
 *                      TS_DEAD
 * ```
 * */
typedef enum {
	TS_INITIALIZED,
	TS_RUNNABLE,
	TS_WAITING,
	TS_LOCKED,
	TS_DEAD
} thread_state_t;

typedef struct {
	plat_thread_cv_t tcv_impl;

	char			tcv_name[TEVENTNAMELEN];
} thread_cv_t;

typedef struct {
	plat_thread_mutex_t tm_impl;

	char 			tm_name[TMUTEXNAMELEN];
	boolean_t		tm_is_recursive;
} thread_mutex_t;

typedef struct {
	plat_thread_rwlock_t tl_impl;

	char 			tl_name[TRWLOCKNAMELEN];
} thread_rwlock_t;

typedef struct {
	plat_thread_key_t tk_impl;

	char			tk_name[TKEYNAMELEN];
} thread_key_t;

typedef struct {
	thread_mutex_t	te_mutex;
	thread_cv_t 	te_cv;

	char			te_name[TEVENTNAMELEN];
} thread_event_t;

/**
 * Default object initializers */
#define THREAD_EVENT_INITIALIZER 							\
	{ SM_INIT(.te_impl, PLAT_THREAD_EVENT_INITIALIZER),		\
	  SM_INIT(.te_name, "\0") }
#define THREAD_MUTEX_INITIALIZER 							\
	{ SM_INIT(.tm_impl, PLAT_THREAD_MUTEX_INITIALIZER),		\
	  SM_INIT(.tm_name, "\0"),								\
	  SM_INIT(.tm_is_recursive, B_FALSE) }
#define THREAD_KEY_INITIALIZER 								\
	{ SM_INIT(.tk_impl, PLAT_THREAD_KEY_INITIALIZER),		\
	  SM_INIT(.tk_name, "\0") }

typedef unsigned 	thread_id_t;

/**
 * Prototype of thread's function */
typedef thread_result_t (*thread_start_func)(thread_arg_t arg);

/**
 * Default stack size of thread */
#define		TSTACKSIZE		(64 * SZ_KB)

/**
 * Thread structure
 *
 * @member t_state 		current state of thread. May be accessed atomically
 * @member t_id			unique thread id assigned by implementation library \
 * 						(i.e. pthreads)
 * @member t_local_id   local thread id assigned by user code
 * @member t_system_id	platform-dependent thread id (optional).
 * @member t_name		name of thread
 * @member t_arg		argument to a thread (processed in THREAD_ENTRY)
 * @member t_ret_code	return code of thread
 */
typedef struct thread {
	plat_thread_t	t_impl;
	plat_sched_t	t_sched_impl;

	atomic_t		t_state_atomic;

	thread_id_t  	t_id;
	int				t_local_id;

	unsigned long   t_system_id;

	char 			t_name[TNAMELEN];

	/* When thread finishes, it notifies parent thread thru t_event
	 * FIXME: Rewrite with cv/mutex, non-atomic t_state */
	thread_event_t*	t_event;

	void*			t_arg;
	unsigned		t_ret_code;

#ifdef TS_LOCK_DEBUG
	ts_time_t		t_block_time;
	thread_cv_t*	t_block_cv;
	thread_mutex_t* t_block_mutex;
	thread_mutex_t* t_block_rwlock;
#endif

	struct thread*	t_next;			/*< Next thread in global thread list*/
	struct thread*	t_pool_next;	/*< Next thread in pool*/
} thread_t;

/**
 * Mutexes
 *
 * @note rmutex_init() is deprecated
 */
LIBEXPORT void mutex_init(thread_mutex_t* mutex, const char* namefmt, ...);
LIBEXPORT void rmutex_init(thread_mutex_t* mutex, const char* namefmt, ...);
LIBEXPORT boolean_t mutex_try_lock(thread_mutex_t* mutex);
LIBEXPORT void mutex_lock(thread_mutex_t* mutex);
LIBEXPORT void mutex_unlock(thread_mutex_t* mutex);
LIBEXPORT void mutex_destroy(thread_mutex_t* mutex);

/**
 * Read-write locks
 */
LIBEXPORT void rwlock_init(thread_rwlock_t* rwlock, const char* namefmt, ...);
LIBEXPORT void rwlock_lock_read(thread_rwlock_t* rwlock);
LIBEXPORT void rwlock_lock_write(thread_rwlock_t* rwlock);
LIBEXPORT void rwlock_unlock(thread_rwlock_t* rwlock);
LIBEXPORT void rwlock_destroy(thread_rwlock_t* rwlock);

/**
 * Condition variables
 */
LIBEXPORT void cv_init(thread_cv_t* cv, const char* namefmt, ...);
LIBEXPORT void cv_wait(thread_cv_t* cv, thread_mutex_t* mutex);
LIBEXPORT void cv_wait_timed(thread_cv_t* cv, thread_mutex_t* mutex, ts_time_t timeout);
LIBEXPORT void cv_notify_one(thread_cv_t* cv);
LIBEXPORT void cv_notify_all(thread_cv_t* cv);
LIBEXPORT void cv_destroy(thread_cv_t* cv);

/**
 * Events
 *
 * @note this API is deprecated. Use condition variable + mutex pair
 */
LIBEXPORT void event_init(thread_event_t* event, const char* namefmt, ...);
LIBEXPORT void event_wait(thread_event_t* event);
LIBEXPORT void event_wait_timed(thread_event_t* event, ts_time_t timeout);
LIBEXPORT void event_notify_one(thread_event_t* event);
LIBEXPORT void event_notify_all(thread_event_t* event);
LIBEXPORT void event_destroy(thread_event_t* event);

/**
 * Thread-local storage
 */
LIBEXPORT void tkey_init(thread_key_t* key,
			   	   	   	 const char* namefmt, ...);
LIBEXPORT void tkey_destroy(thread_key_t* key);
LIBEXPORT void tkey_set(thread_key_t* key, void* value);
LIBEXPORT void* tkey_get(thread_key_t* key);

LIBEXPORT thread_t* t_self();

LIBEXPORT void t_init(thread_t* thread, void* arg,
					  thread_start_func start,
		              const char* namefmt, ...);
LIBEXPORT thread_t* t_post_init(thread_t* t);
LIBEXPORT void t_exit(thread_t* t);
LIBEXPORT void t_destroy(thread_t* thread);
LIBEXPORT void t_wait_start(thread_t* thread);
LIBEXPORT void t_join(thread_t* thread);

LIBEXPORT int threads_init(void);
LIBEXPORT void threads_fini(void);

LIBEXPORT PLATAPI void t_eternal_wait(void);

LIBEXPORT PLATAPI long t_get_pid(void);

/* Platform-dependent functions */

PLATAPI void plat_thread_init(plat_thread_t* thread, void* arg,
							  thread_start_func start);
PLATAPI void plat_thread_destroy(plat_thread_t* thread);
PLATAPI void plat_thread_join(plat_thread_t* thread);
PLATAPI unsigned long plat_gettid();

PLATAPI void plat_mutex_init(plat_thread_mutex_t* mutex, boolean_t recursive);
PLATAPI boolean_t plat_mutex_try_lock(plat_thread_mutex_t* mutex);
PLATAPI void plat_mutex_lock(plat_thread_mutex_t* mutex);
PLATAPI void plat_mutex_unlock(plat_thread_mutex_t* mutex);
PLATAPI void plat_mutex_destroy(plat_thread_mutex_t* mutex);

PLATAPI void plat_rwlock_init(plat_thread_rwlock_t* rwlock);
PLATAPI void plat_rwlock_lock_read(plat_thread_rwlock_t* rwlock);
PLATAPI void plat_rwlock_lock_write(plat_thread_rwlock_t* rwlock);
PLATAPI void plat_rwlock_unlock(plat_thread_rwlock_t* rwlock);
PLATAPI void plat_rwlock_destroy(plat_thread_rwlock_t* rwlock);

PLATAPI void plat_cv_init(plat_thread_cv_t* cv);
PLATAPI void plat_cv_wait_timed(plat_thread_cv_t* cv, plat_thread_mutex_t* mutex, ts_time_t timeout);
PLATAPI void plat_cv_notify_one(plat_thread_cv_t* cv);
PLATAPI void plat_cv_notify_all(plat_thread_cv_t* cv);
PLATAPI void plat_cv_destroy(plat_thread_cv_t* cv);

PLATAPI void plat_tkey_init(plat_thread_key_t* key);
PLATAPI void plat_tkey_destroy(plat_thread_key_t* key);
PLATAPI void plat_tkey_set(plat_thread_key_t* key, void* value);
PLATAPI void* plat_tkey_get(plat_thread_key_t* key);

#endif /* THREADPOOL_H_ */
