/*
 * tsinit.c
 *
 *  Created on: 30.12.2012
 *      Author: myaut
 */

#include <tsinit.h>

#include <stdio.h>
#include <stdlib.h>

int tsi_subsys_count;
struct subsystem** tsi_subsys = NULL;

void ts_finish(void);

int ts_init(struct subsystem** subsys_list, int count) {
	int i = 0;
	int err = 0;

	tsi_subsys = subsys_list;
	tsi_subsys_count = count;

	if(plat_init() == -1) {
		fprintf(stderr, "Platform-dependent initialization failure!\n");
		exit(-1);
	}

	for(i = 0; i < count; ++i) {
		err = tsi_subsys[i]->s_init();

		if(err != 0) {
			tsi_subsys[i]->s_state = SS_ERROR;
			fprintf(stderr, "Failure initializing %s, exiting\n", tsi_subsys[i]->s_name);
			exit(err);

			return err;
		}

		tsi_subsys[i]->s_error_code = err;
	}

	return 0;
}

void ts_finish(void) {
	int i = 0;

	/* Not yet initialized */
	if(!tsi_subsys)
		return;

	for(i = tsi_subsys_count - 1; i >= 0; --i) {
		if(tsi_subsys[i]->s_state == SS_OK) {
			tsi_subsys[i]->s_fini();
		}
	}

	free(tsi_subsys);
	tsi_subsys = NULL;

	plat_finish();
}
