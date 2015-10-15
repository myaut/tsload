
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

#include <tsload/signal.h>

#include <windows.h>

signal_func handler = NULL;

BOOL real_handler(DWORD ev) {
    if(handler != NULL)
        handler(ev);
    return TRUE;
}

PLATAPI int register_signal_func(signal_t sig, signal_func func) {
    if(sig != SIGNAL_INTERRUPT)
        return -1;
    
    handler = func;
    if(SetConsoleCtrlHandler(real_handler, TRUE))
        return 0;
    return -1;
}

PLATAPI int reset_signal_func(signal_t sig) {
    if(sig != SIGNAL_INTERRUPT)
        return -1;
    
    handler = NULL;
    if(SetConsoleCtrlHandler(real_handler, FALSE))
        return 0;
    return -1;
}