/*
 * plat.c
 *
 *  Created on: 20.12.2012
 *      Author: myaut
 */

#include <defs.h>

#include <tstime.h>
#include <plat/win/threads.h>

#include <assert.h>

/* Mutexes */

PLATAPI void plat_mutex_init(plat_thread_mutex_t* mutex, boolean_t recursive) {
	InitializeCriticalSection(&mutex->tm_crit_section);

	/* Recursive mutexes are not supported*/
	assert(!recursive);
}

PLATAPI void plat_mutex_lock(plat_thread_mutex_t* mutex) {
	EnterCriticalSection(&mutex->tm_crit_section);
}

PLATAPI boolean_t plat_mutex_try_lock(plat_thread_mutex_t* mutex) {
	return TryEnterCriticalSection(&mutex->tm_crit_section) != 0;
}

PLATAPI void plat_mutex_unlock(plat_thread_mutex_t* mutex) {
	LeaveCriticalSection(&mutex->tm_crit_section);
}

PLATAPI void plat_mutex_destroy(plat_thread_mutex_t* mutex) {
	DeleteCriticalSection(&mutex->tm_crit_section);
}

/* Events */

PLATAPI void plat_cv_init(plat_thread_cv_t* cv) {
	InitializeConditionVariable(&cv->tcv_cond_var);
}

PLATAPI void plat_cv_wait_timed(plat_thread_cv_t* cv, plat_thread_mutex_t* mutex, ts_time_t timeout) {
	DWORD millis;

	millis = (timeout == TS_TIME_MAX)
					? INFINITE
					: timeout / T_MS;

	SleepConditionVariableCS(&cv->tcv_cond_var,
							 &mutex->tm_crit_section,
							 millis);
}

PLATAPI void plat_cv_notify_one(plat_thread_cv_t* cv) {
	WakeConditionVariable(&cv->tcv_cond_var);
}

PLATAPI void plat_cv_notify_all(plat_thread_cv_t* cv) {
	WakeAllConditionVariable(&cv->tcv_cond_var);
}

PLATAPI void plat_cv_destroy(plat_thread_cv_t* cv) {
	/* Do nothing */
}

/* Keys */
PLATAPI void plat_tkey_init(plat_thread_key_t* key) {
	key->tk_tls = TlsAlloc();

	assert(key->tk_tls != TLS_OUT_OF_INDEXES);
}

PLATAPI void plat_tkey_destroy(plat_thread_key_t* key) {
	TlsFree(key->tk_tls);
}

PLATAPI void plat_tkey_set(plat_thread_key_t* key, void* value) {
	TlsSetValue(key->tk_tls, value);
}

PLATAPI void* plat_tkey_get(plat_thread_key_t* key) {
	return TlsGetValue(key->tk_tls);
}

/* RW locks */

PLATAPI void plat_rwlock_init(plat_thread_rwlock_t* rwlock) {
	InitializeSRWLock(&rwlock->tl_slim_rwlock);

	/* Because WinAPI provides two functions to release lock,
	 * save mode of lock acquiring */
	rwlock->tl_mode_key = TlsAlloc();
	assert(rwlock->tl_mode_key != TLS_OUT_OF_INDEXES);
}

PLATAPI void plat_rwlock_lock_read(plat_thread_rwlock_t* rwlock) {
	AcquireSRWLockShared(&rwlock->tl_slim_rwlock);

	TlsSetValue(rwlock->tl_mode_key, (void*) TL_MODE_SHARED);
}

PLATAPI void plat_rwlock_lock_write(plat_thread_rwlock_t* rwlock) {
	AcquireSRWLockExclusive(&rwlock->tl_slim_rwlock);

	TlsSetValue(rwlock->tl_mode_key, (void*) TL_MODE_EXCLUSIVE);
}

PLATAPI void plat_rwlock_unlock(plat_thread_rwlock_t* rwlock) {
	switch((unsigned) TlsGetValue(rwlock->tl_mode_key)) {
	case TL_MODE_SHARED:
		ReleaseSRWLockShared(&rwlock->tl_slim_rwlock);
		break;
	case TL_MODE_EXCLUSIVE:
		ReleaseSRWLockExclusive(&rwlock->tl_slim_rwlock);
		break;
	default:
		abort();
	}

	TlsSetValue(rwlock->tl_mode_key, (void*) TL_MODE_FREE);
}

PLATAPI void plat_rwlock_destroy(plat_thread_rwlock_t* rwlock) {
	TlsFree(rwlock->tl_mode_key);
}
