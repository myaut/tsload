
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



#define LOG_SOURCE "thread"
#include <tsload/defs.h>

#include <tsload/log.h>
#include <tsload/threads.h>
#include <tsload/hashmap.h>
#include <tsload/atomic.h>
#include <tsload/schedutil.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>


/* Thread hash map. Maps pthreads to our threads */
DECLARE_HASH_MAP(thread_hash_map, B_FALSE, thread_t, THASHSIZE, t_id, t_next,
	/*hash*/ {
		thread_id_t tid = * (thread_id_t*) key;

		return tid % THASHMASK;
	},
	/*compare*/ {
		thread_id_t tid1 = * (thread_id_t*) key1;
		thread_id_t tid2 = * (thread_id_t*) key2;

		return tid1 == tid2;
	});

/*End of thread_hash_map declaration*/

thread_key_t thread_key;

/**
 * returns pointer to current thread *
 * Used to monitor mutex/event deadlock and starvation (see tutil.c)
 *  */
thread_t* t_self() {
	return (thread_t*) tkey_get(&thread_key);
}

/*
 * Generate next thread id */
static thread_id_t t_assign_id() {
	static thread_id_t tid = 0;
	return ++tid;
}

void t_notify_state(thread_t* t, thread_state_t state) {
	plat_mutex_lock(&t->t_mutex);
	t->t_state = state;
	plat_cv_notify_all(&t->t_condvar);
	plat_mutex_unlock(&t->t_mutex);
}

/**
 * Initialize and run thread
 *
 * @param thread	 pointer to thread
 * @param arg 		 argument passed to thread
 * @param start      thread function
 * @param namefmt	 format string and it's arguments for thread name
 * */
void t_init(thread_t* thread, void* arg,
		thread_start_func start,
		const char* namefmt, ...) {
	va_list va;

	thread->t_id = t_assign_id();
	thread->t_local_id = 0;

	va_start(va, namefmt);
	vsnprintf(thread->t_name, TNAMELEN, namefmt, va);
	va_end(va);

	thread->t_arg = arg;

	thread->t_state = TS_INITIALIZED;

	thread->t_next = NULL;
	thread->t_pool_next = NULL;

	thread->t_system_id = 0;

	thread->t_ret_code = 0;

	logmsg(LOG_DEBUG, "Created thread %d '%s'", thread->t_id, thread->t_name);

	plat_mutex_init(&thread->t_mutex, B_FALSE);
	plat_cv_init(&thread->t_condvar);
	
	plat_thread_init(&thread->t_impl, (void*) thread, start);
	plat_tsched_init(thread);
}

/**
 * Post-initialize thread. Called from context of thread by THREAD_ENTRY
 * Inserts it to hash-map and sets platform thread id if possible
 *
 * @param t - current thread
 * @return t (for THREAD_ENTRY)
 *
 * @note Do not call this function directly, use THREAD_ENTRY macro
 * */
thread_t* t_post_init(thread_t* t) {
	t->t_system_id = plat_gettid();

	hash_map_insert(&thread_hash_map, t);

	tkey_set(&thread_key, (void*) t);

	t_notify_state(t, TS_RUNNABLE);

	return t;
}

/**
 * Exit from thread. Called from context of thread: notifies thread which is joined to t
 * and remove thread from hash_map.
 *
 * @note Do not call this function directly, use THREAD_EXIT macro
 * */
void t_exit(thread_t* t) {
	logmsg(LOG_DEBUG, "Thread %d '%s' exited", t->t_id, t->t_name);

	t_notify_state(t, TS_DEAD);
}

/**
 * Wait until thread starts
 * */
void t_wait_start(thread_t* t) {
	plat_mutex_lock(&t->t_mutex);
	
	while(t->t_state == TS_INITIALIZED)
		plat_cv_wait_timed(&t->t_condvar, &t->t_mutex, TS_TIME_MAX);
	
	plat_mutex_unlock(&t->t_mutex);
}

/**
 * Wait until thread finishes
 * */
void t_join(thread_t* thread) {
	thread_state_t state;
		
	plat_mutex_lock(&thread->t_mutex);
	state = thread->t_state;
	plat_mutex_unlock(&thread->t_mutex);
	
	if(state != TS_DEAD) {
		plat_thread_join(&thread->t_impl);
	}
}

/**
 * Destroy thread. If thread is still alive - join to it.
 *
 * @note blocks until thread exits from itself!
 * */
void t_destroy(thread_t* thread) {
	t_join(thread);
	
	plat_mutex_destroy(&thread->t_mutex);
	plat_cv_destroy(&thread->t_condvar);
	
	hash_map_remove(&thread_hash_map, thread);
	plat_thread_destroy(&thread->t_impl);
	plat_tsched_destroy(thread);
}

#ifdef THREAD_DUMP_ENABLED
static int t_dump_thread(void* object, void* arg) {
	const char* t_state_name[] = {
			"INIT",
			"RUNA",
			"WAIT",
			"LOCK",
			"DEAD"
	};
	char t_wait_state[64];
	thread_t* t = (thread_t*) object;
	struct timeval t_wait_tv;

	if(t->t_state == TS_WAITING ||
	   t->t_state == TS_LOCKED) {
		char* w_name = (t->t_state == TS_WAITING)
					  	? t->t_block_event->te_name
						: t->t_block_mutex->tm_name;
		char* w_state = (t->t_state == TS_WAITING)
						? "Waiting"
						: "Locked";

		t_get_wait_time(t, &t_wait_tv);

		snprintf(t_wait_state, 64, "(%s, %s, %ld.%06ld)",
					w_state, w_name,
					t_wait_tv.tv_sec,
					t_wait_tv.tv_usec);
	}
	else {
		strcpy(t_wait_state, "(not-blocked)");
	}

	logmsg(LOG_DEBUG, "%5d %5d %32s %5s %s",
			t->t_id, t->t_system_id,
			t->t_name, t_state_name[t->t_state],
			t_wait_state);

	return HM_WALKER_CONTINUE;
}

/**
 * Logs how many time each thread spent on event/mutex
 * */
void t_dump_threads() {
	logmsg(LOG_DEBUG, "%5s %5s %32s %5s %s", "TID", "STID", "NAME", "STATE", "BLOCK");

	(void) hash_map_walk(&thread_hash_map, t_dump_thread, NULL);
}
#endif

int threads_init(void) {
	tkey_init(&thread_key, "thread_key");

	hash_map_init(&thread_hash_map, "thread_hash_map");

	return 0;
}

void threads_fini(void) {
	hash_map_destroy(&thread_hash_map);

	tkey_destroy(&thread_key);
}

