
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#ifndef ILOG2_H
#define ILOG2_H

#include <tsload/defs.h>

#include <stdint.h>
#include <limits.h>


#if defined(__GNUC__)

STATIC_INLINE int __msb32(uint32_t i) {
	return (sizeof(unsigned long) * CHAR_BIT - 1) -
			__builtin_clzl(i);
}

STATIC_INLINE int __msb64(uint64_t i) {
	return (sizeof(unsigned long long) * CHAR_BIT - 1) -
			__builtin_clzll(i);
}

#elif defined(_MSC_VER)

#include <intrin.h>

STATIC_INLINE int __msb32(uint32_t i) {
	unsigned long index;
	 _BitScanReverse(&index, i);
	return index;
}

STATIC_INLINE int __msb64(uint64_t i) {
	unsigned long index;
#if defined(_M_X64) || defined(_M_AMD64)
	_BitScanReverse64(&index, i);
#else
	if((i >> 32) == 0) {
		_BitScanReverse(&index, (uint32_t) (i & 0xFFFFFFFF));
	}
	else {
		_BitScanReverse(&index, (uint32_t) (i >> 32));
		index += 32;
	}
#endif
	return index;
}

#else

/* Implement slower versions */
STATIC_INLINE int __msb32(uint32_t i) {
	int ret = 0;

	while((i & (1 << 31)) == 0) {
		i <<= 1;
		++ret;
	}

	return ret;
}

STATIC_INLINE int __msb64(uint64_t i) {
	int ret = 0;

	while((i & (1 << 63)) == 0) {
		i <<= 1;
		++ret;
	}

	return ret;
}

#endif

/**
 * Calculate log2(value) for 32 bit integers
 *
 * @return -1 if value == 0 or log2(value) */
STATIC_INLINE int ilog2l(unsigned long value) {
	if(value == 0)
		return -1;

	return __msb32(value) + ((value & (value - 1)) != 0);
}

/**
 * Calculate log2(value) for 64 bit integers
 *
 * @return -1 if value == 0 or log2(value) */
STATIC_INLINE int ilog2ll(unsigned long long value) {
	if(value == 0)
		return -1;

	return __msb64(value) + ((value & (value - 1)) != 0);
}

#endif /* TSUTIL_H_ */

