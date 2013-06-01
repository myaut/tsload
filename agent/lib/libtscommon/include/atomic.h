/*
 * atomic.h
 *
 *  Created on: 05.12.2012
 *      Author: myaut
 */

#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <defs.h>

#if defined(HAVE_ATOMIC_BUILTINS)

typedef volatile long atomic_t;

STATIC_INLINE long atomic_read(atomic_t* atom) {
	return __sync_fetch_and_add(atom, 0);
}

STATIC_INLINE void atomic_set(atomic_t* atom, long value) {
	 (void) __sync_lock_test_and_set(atom, value);
}

STATIC_INLINE long atomic_exchange(atomic_t* atom, long value) {
	 return __sync_lock_test_and_set(atom, value);
}

STATIC_INLINE long atomic_inc(atomic_t* atom) {
	return __sync_fetch_and_add(atom, 1);
}

STATIC_INLINE long atomic_dec(atomic_t* atom) {
	return __sync_fetch_and_sub(atom, 1);
}

STATIC_INLINE long atomic_add(atomic_t* atom, long value) {
	return __sync_fetch_and_add(atom, value);
}

STATIC_INLINE long atomic_sub(atomic_t* atom, long value) {
	return __sync_fetch_and_sub(atom, value);
}

STATIC_INLINE long atomic_or(atomic_t* atom, long value) {
	return __sync_fetch_and_or(atom, value);
}

STATIC_INLINE long atomic_and(atomic_t* atom, long value) {
	return __sync_fetch_and_and(atom, value);
}


#elif defined(PLAT_WIN) && defined(_MSC_VER)

#include <windows.h>

#if !defined(_M_AMD64) && !defined(_M_IA64) && !defined(_M_X64)
#include <intrin.h>
#pragma intrinsic (_InterlockedAnd)
#define InterlockedAnd _InterlockedAnd
#endif

typedef LONG volatile atomic_t;

STATIC_INLINE long atomic_read(atomic_t* atom) {
	return InterlockedExchangeAdd(atom, 0);
}

STATIC_INLINE void atomic_set(atomic_t* atom, long value) {
	(void) InterlockedExchange(atom, value);
}

STATIC_INLINE long atomic_exchange(atomic_t* atom, long value) {
	return InterlockedExchange(atom, value);
}

STATIC_INLINE long atomic_add(atomic_t* atom, long value) {
	return InterlockedExchangeAdd(atom, value);
}

STATIC_INLINE long atomic_sub(atomic_t* atom, long value) {
	return InterlockedExchangeAdd(atom, -value);
}

STATIC_INLINE long atomic_inc(atomic_t* atom) {
	return InterlockedIncrement(atom);
}

STATIC_INLINE long atomic_dec(atomic_t* atom) {
	return InterlockedDecrement(atom);
}

STATIC_INLINE long atomic_or(atomic_t* atom, long value) {
	return InterlockedOr(atom, value);
}

STATIC_INLINE long atomic_and(atomic_t* atom, long value) {
	return InterlockedAnd(atom, value);
}


#else

#error "Atomic operations are not supported for your platform"

#endif /* __GNUC__*/

#endif /* ATOMIC_H_ */
