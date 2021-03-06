
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



#include <tsload/defs.h>

#include <tsload/threads.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


#ifdef TS_LOCK_DEBUG
#define THREAD_ENTER_LOCK(state, objname, obj) 	\
	thread_t* t = t_self();						\
	if(t != NULL) {								\
		t->t_block_time = tm_get_time();		\
		t_notify_state(t, state);				\
		t->t_block_##objname = obj;				\
	}

#define THREAD_LEAVE_LOCK(objname)						\
	if(t != NULL) {										\
		t_notify_state(t, TS_RUNNABLE);					\
		t->t_block_##objname = NULL;					\
	}
#else
#define THREAD_ENTER_LOCK(state, objname, obj)
#define THREAD_LEAVE_LOCK(objname)
#endif

/* Condition variables
 * ------ */

void cv_init(thread_cv_t* cv, const char* namefmt, ...) {
	va_list va;

	plat_cv_init(&cv->tcv_impl);

	va_start(va, namefmt);
	vsnprintf(cv->tcv_name, TCVNAMELEN, namefmt, va);
	va_end(va);
}

void cv_wait(thread_cv_t* cv, thread_mutex_t* mutex) {
	cv_wait_timed(cv, mutex, TS_TIME_MAX);
}

void cv_wait_timed(thread_cv_t* cv, thread_mutex_t* mutex, ts_time_t timeout) {
	THREAD_ENTER_LOCK(TS_WAITING, cv, cv);

	plat_cv_wait_timed(&cv->tcv_impl, &mutex->tm_impl, timeout);

	THREAD_LEAVE_LOCK(cv);
}


void cv_notify_one(thread_cv_t* cv) {
	plat_cv_notify_one(&cv->tcv_impl);
}

void cv_notify_all(thread_cv_t* cv) {
	plat_cv_notify_all(&cv->tcv_impl);
}

void cv_destroy(thread_cv_t* cv) {
	plat_cv_destroy(&cv->tcv_impl);
}

/* Mutexes
 * ------- */

static void __mutex_init(thread_mutex_t* mutex,
						 int recursive,
					     const char* namefmt, va_list va) {
	plat_mutex_init(&mutex->tm_impl, recursive);

	vsnprintf(mutex->tm_name, TMUTEXNAMELEN, namefmt, va);
}

void mutex_init(thread_mutex_t* mutex, const char* namefmt, ...) {
	va_list va;

	va_start(va, namefmt);
	__mutex_init(mutex, B_FALSE, namefmt, va);
	va_end(va);
}

void rmutex_init(thread_mutex_t* mutex, const char* namefmt, ...) {
	va_list va;

	va_start(va, namefmt);
	__mutex_init(mutex, B_TRUE, namefmt, va);
	va_end(va);
}


void mutex_lock(thread_mutex_t* mutex) {
	THREAD_ENTER_LOCK(TS_LOCKED, mutex, mutex);

	plat_mutex_lock(&mutex->tm_impl);

	THREAD_LEAVE_LOCK(mutex);
}

boolean_t mutex_try_lock(thread_mutex_t* mutex) {
	return plat_mutex_try_lock(&mutex->tm_impl);
}

void mutex_unlock(thread_mutex_t* mutex) {
	plat_mutex_unlock(&mutex->tm_impl);
}

void mutex_destroy(thread_mutex_t* mutex) {
	plat_mutex_destroy(&mutex->tm_impl);
}

/* Keys
 * ---- */

void tkey_init(thread_key_t* key,
			   const char* namefmt, ...) {
	va_list va;

	plat_tkey_init(&key->tk_impl);

	va_start(va, namefmt);
	vsnprintf(key->tk_name, TKEYNAMELEN, namefmt, va);
	va_end(va);
}

void tkey_destroy(thread_key_t* key) {
	plat_tkey_destroy(&key->tk_impl);
}

void tkey_set(thread_key_t* key, void* value) {
	plat_tkey_set(&key->tk_impl, value);
}

void* tkey_get(thread_key_t* key) {
	return plat_tkey_get(&key->tk_impl);
}

/* Reader-writer locks
 * ------------------- */

void rwlock_init(thread_rwlock_t* rwlock, const char* namefmt, ...) {
	va_list va;

	plat_rwlock_init(&rwlock->tl_impl);

	va_start(va, namefmt);
	vsnprintf(rwlock->tl_name, TRWLOCKNAMELEN, namefmt, va);
	va_end(va);
}

void rwlock_lock_read(thread_rwlock_t* rwlock) {
	THREAD_ENTER_LOCK(TS_LOCKED, rwlock, rwlock);

	plat_rwlock_lock_read(&rwlock->tl_impl);

	THREAD_LEAVE_LOCK(rwlock);
}

void rwlock_lock_write(thread_rwlock_t* rwlock) {
	THREAD_ENTER_LOCK(TS_LOCKED, rwlock, rwlock);

	plat_rwlock_lock_write(&rwlock->tl_impl);

	THREAD_LEAVE_LOCK(rwlock);
}

void rwlock_unlock(thread_rwlock_t* rwlock) {
	plat_rwlock_unlock(&rwlock->tl_impl);
}

void rwlock_destroy(thread_rwlock_t* rwlock) {
	plat_rwlock_destroy(&rwlock->tl_impl);
}
