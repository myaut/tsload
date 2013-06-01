/*
 * threads.c
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#include <defs.h>

PLATAPI unsigned long plat_gettid() {
#ifdef HAVE_DECL__LWP_SELF
#	include <sys/lwp.h>
	return _lwp_self();
#else
	return 0;
#endif
}
