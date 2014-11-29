
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



#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <tsload/defs.h>


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

#elif defined(HAVE_SYNC_BUILTINS)

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

