/*
 * plat.c
 *
 *  Created on: 20.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <tstime.h>
#include <plat/posix/threads.h>

#include <errno.h>
#include <pthread.h>

#include <assert.h>

/* Mutexes */

PLATAPI void plat_mutex_init(plat_thread_mutex_t* mutex, int recursive) {
	pthread_mutexattr_t mta;

	pthread_mutexattr_init(&mta);

	if(recursive)
		pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
	else
		pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_NORMAL);

	pthread_mutex_init(&mutex->tm_mutex, &mta);
}

PLATAPI boolean_t plat_mutex_try_lock(plat_thread_mutex_t* mutex) {
	return (pthread_mutex_trylock(&mutex->tm_mutex) == 0);
}

PLATAPI void plat_mutex_lock(plat_thread_mutex_t* mutex) {
	pthread_mutex_lock(&mutex->tm_mutex);
}

PLATAPI void plat_mutex_unlock(plat_thread_mutex_t* mutex) {
	pthread_mutex_unlock(&mutex->tm_mutex);
}

PLATAPI void plat_mutex_destroy(plat_thread_mutex_t* mutex) {
	pthread_mutex_destroy(&mutex->tm_mutex);
}

/* Events */

PLATAPI void plat_cv_init(plat_thread_cv_t* cv) {
	pthread_cond_init (&cv->tcv_cv, NULL);
}

PLATAPI void plat_cv_wait_timed(plat_thread_cv_t* cv, plat_thread_mutex_t* mutex, ts_time_t timeout) {
	if(timeout == TS_TIME_MAX) {
		pthread_cond_wait(&cv->tcv_cv, &mutex->tm_mutex);
	}
	else {
		struct timespec ts;

		ts.tv_sec = TS_TIME_SEC(timeout);
		ts.tv_nsec = timeout - (ts.tv_sec * T_SEC);

		pthread_cond_timedwait(&cv->tcv_cv, &mutex->tm_mutex, &ts);
	}
}

PLATAPI void plat_cv_notify_one(plat_thread_cv_t* cv) {
    pthread_cond_signal(&cv->tcv_cv);
}

PLATAPI void plat_cv_notify_all(plat_thread_cv_t* cv) {
    pthread_cond_broadcast(&cv->tcv_cv);
}

PLATAPI void plat_cv_destroy(plat_thread_cv_t* cv) {
	pthread_cond_destroy(&cv->tcv_cv);
}

/* Keys */
PLATAPI void plat_tkey_init(plat_thread_key_t* key) {
	pthread_key_create(&key->tk_key, NULL);
}

PLATAPI void plat_tkey_destroy(plat_thread_key_t* key) {
	pthread_key_delete(key->tk_key);
}

PLATAPI void plat_tkey_set(plat_thread_key_t* key, void* value) {
	pthread_setspecific(key->tk_key, value);
}

PLATAPI void* plat_tkey_get(plat_thread_key_t* key) {
	return pthread_getspecific(key->tk_key);
}

/* RW locks*/

PLATAPI void plat_rwlock_init(plat_thread_rwlock_t* rwlock) {
	pthread_rwlock_init(&rwlock->tl_rwlock, NULL);
}

PLATAPI void plat_rwlock_lock_read(plat_thread_rwlock_t* rwlock) {
	pthread_rwlock_rdlock(&rwlock->tl_rwlock);
}

PLATAPI void plat_rwlock_lock_write(plat_thread_rwlock_t* rwlock) {
	pthread_rwlock_wrlock(&rwlock->tl_rwlock);
}

PLATAPI void plat_rwlock_unlock(plat_thread_rwlock_t* rwlock) {
	pthread_rwlock_unlock(&rwlock->tl_rwlock);
}

PLATAPI void plat_rwlock_destroy(plat_thread_rwlock_t* rwlock) {
	pthread_rwlock_destroy(&rwlock->tl_rwlock);
}
