
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#ifndef TSINIT_H_
#define TSINIT_H_

#include <defs.h>

typedef enum {
	SS_UNINITIALIZED,
	SS_OK,
	SS_ERROR
} subsystem_state_t;

struct subsystem {
	char* s_name;

	int s_state;
	int s_error_code;

	int (*s_init)(void);
	void (*s_fini)(void);
};

#define SUBSYSTEM(name, init, fini) 		\
	{										\
		SM_INIT(.s_name, name),				\
		SM_INIT(.s_error_code, 0),			\
		SM_INIT(.s_state, SS_UNINITIALIZED),\
		SM_INIT(.s_init, init),				\
		SM_INIT(.s_fini, fini)				\
	}

LIBEXPORT int ts_init(struct subsystem** subsys, int count);
LIBEXPORT void ts_finish(void);

PLATAPI int plat_init(void);
PLATAPI void plat_finish(void);

#endif /* TSINIT_H_ */

