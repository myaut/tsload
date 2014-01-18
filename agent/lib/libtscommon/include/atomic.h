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

#define ATOMIC_MEMMODEL __ATOMIC_SEQ_CST

typedef volatile long atomic_t;

STATIC_INLINE long atomic_read(atomic_t* atom) {
	long value;
	__atomic_load(atom, &value, ATOMIC_MEMMODEL);
	return value;
}

STATIC_INLINE long atomic_exchange(atomic_t* atom, long value) {
	 return  __atomic_exchange_n(atom, value, ATOMIC_MEMMODEL);
}

STATIC_INLINE void atomic_set(atomic_t* atom, long value) {
	__atomic_store(atom, &value, ATOMIC_MEMMODEL);
}

STATIC_INLINE long atomic_inc(atomic_t* atom) {
	return __atomic_add_fetch(atom, 1, ATOMIC_MEMMODEL) - 1;
}

STATIC_INLINE long atomic_dec(atomic_t* atom) {
	return __atomic_sub_fetch(atom, 1, ATOMIC_MEMMODEL) + 1;
}

STATIC_INLINE long atomic_add(atomic_t* atom, long value) {
	return  __atomic_add_fetch(atom, value, ATOMIC_MEMMODEL) - value;
}

STATIC_INLINE long atomic_sub(atomic_t* atom, long value) {
	return  __atomic_sub_fetch(atom, value, ATOMIC_MEMMODEL) + value;
}

STATIC_INLINE long atomic_or(atomic_t* atom, long value) {
	return  __atomic_fetch_or(atom, value, ATOMIC_MEMMODEL);
}

STATIC_INLINE long atomic_and(atomic_t* atom, long value) {
	return  __atomic_fetch_and(atom, value, ATOMIC_MEMMODEL);
}


#elif defined(PLAT_WIN) && defined(_MSC_VER)

#include <windows.h>

#if !defined(_M_AMD64) && !defined(_M_IA64) && !defined(_M_X64)
#include <intrin.h>
#pragma intrinsic (_InterlockedAnd)
#define InterlockedAnd _InterlockedAnd

#pragma intrinsic (_InterlockedOr)
#define InterlockedOr _InterlockedOr
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
	return InterlockedIncrement(atom) - 1;
}

STATIC_INLINE long atomic_dec(atomic_t* atom) {
	return InterlockedDecrement(atom) + 1;
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
