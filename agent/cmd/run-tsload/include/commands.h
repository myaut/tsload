/*
 * commands.h
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <defs.h>

#define LOAD_OK				0
#define LOAD_ERR_OPEN_FAIL	-1
#define LOAD_ERR_MMAP_FAIL	-2
#define LOAD_ERR_BAD_JSON	-3
#define LOAD_ERR_JSON_PROC	-4
#define LOAD_ERR_CONFIGURE	-5
#define LOAD_ERR_ENVIRONMENT -6

#define MAXFNLEN	512

#define EXPNAMELEN	32

#define WL_START_DELAY		(3 * T_SEC)

#define DEFAULT_EXPERIMENT_FILENAME		"experiment.json"
#define DEFAULT_EXPID_FILENAME			"experiment.id"

int do_info();
int do_load();

LIBEXPORT int load_init(void);
LIBEXPORT void load_fini(void);


#endif /* COMMANDS_H_ */
