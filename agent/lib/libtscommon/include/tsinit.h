/*
 * tsinit.h
 *
 *  Created on: 30.12.2012
 *      Author: myaut
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
