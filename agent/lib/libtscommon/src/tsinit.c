
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



#include <tsinit.h>
#include <tuneit.h>

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
			tsi_subsys[i]->s_error_code = err;

			tuneit_finalize();

			fprintf(stderr, "Failure initializing %s, exiting\n", tsi_subsys[i]->s_name);
			exit(err);

			/* NOTREACHED */
			return err;
		}

		tsi_subsys[i]->s_state = SS_OK;
		tsi_subsys[i]->s_error_code = 0;
	}

	tuneit_finalize();

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

