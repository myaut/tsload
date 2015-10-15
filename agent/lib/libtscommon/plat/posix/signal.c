
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

#include <signal.h>

static int posix_signum(signal_t sig) {
    switch(sig) {
        case SIGNAL_INTERRUPT:
            return SIGINT;
        case SIGNAL_TERMINATE:
            return SIGTERM;
    }
    return -1;
}

PLATAPI int register_signal_func(signal_t sig, signal_func func) {
    int signum = posix_signum(sig);
    
    if(signum == -1 || signal(signum, func) == SIG_ERR) {
        return -1;
    }
    
    return 0;
}

PLATAPI int reset_signal_func(signal_t sig) {
    int signum = posix_signum(sig);
    
    if(signum == -1 || signal(signum, SIG_DFL) == SIG_ERR) {
        return -1;
    }
    
    return 0;
}