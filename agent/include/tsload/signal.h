
/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus

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

#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <tsload/defs.h>

/**
 * @module Signal handling
 * 
 * Simple module for handling fatal signals like interrupt
 */

typedef enum {
    SIGNAL_INTERRUPT,
    SIGNAL_TERMINATE
} signal_t;

/**
 * Hook for signal
 */
typedef void (*signal_func)(int signum);

PLATAPI LIBEXPORT int register_signal_func(signal_t sig, signal_func func);
PLATAPI LIBEXPORT int reset_signal_func(signal_t sig);

#endif