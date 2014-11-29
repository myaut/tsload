
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



#include <tsload/defs.h>

#include <tsload/getopt.h>
#include <tsload/pathutil.h>

#include <swatfile.h>
#include <swat.h>
#include <swatmap.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <zlib.h>


extern char flat_filename[];
extern char experiment_dir[];

extern boolean_t conv_endianess;

extern swat_command_t command;
extern uint64_t swat_stat_device;

void usage() {
	fprintf(stderr, "swat-import - tool for converting flatfile.bin => run-tsload format"
					"command line: \n"
					"\tswat-import [-c] -f <flatfile.bin.gz> -e <experiment directory> -m <dev=path>\n"
					"\t\t process flatfile\n"
					"\t\t -c - convert endianess\n"
					"\tswat-import [-c] -f <flatfile.bin.gz> -s <dev>\n"
					"\t\tprint statistics from swat-file\n"
					"\tswat-import -h \n"
					"\t\thelp\n");
}

void parse_options(int argc, const char* argv[]) {
	boolean_t eflag = B_FALSE;
	boolean_t fflag = B_FALSE;
	boolean_t sflag = B_FALSE;
	boolean_t ok = B_TRUE;

	time_t now;

	char* map_arg;
	uint64_t dev;

	int c;

	char logname[48];

	while((c = plat_getopt(argc, argv, "e:f:m:chs:")) != -1) {
		switch(c) {
		case 'h':
			usage();
			exit(0);
			break;
		case 's':
			sflag = B_TRUE;
			/* FIXME: atoll not supports uint64_t */
			swat_stat_device = atoll(optarg);
			command = CMD_STAT;
			break;
		case 'c':
			conv_endianess = B_TRUE;
			break;
		case 'e':
			eflag = B_TRUE;
			strncpy(experiment_dir, optarg, PATHMAXLEN);
			command = CMD_SER;
			break;
		case 'm':
			map_arg = strchr(optarg, '=');

			if(map_arg == NULL) {
				fprintf(stderr, "-m argument should contain equal sign.\n");
				ok = B_FALSE;
				break;
			}

			*map_arg++ = '\0';
			/* FIXME: atoll not supports uint64_t */
			dev = atoll(optarg);
			swat_add_mapping(dev, map_arg);

			break;
		case 'f':
			fflag = B_TRUE;
			strncpy(flat_filename, optarg, PATHMAXLEN);
			break;
		case '?':
			if(optopt == 'e' || optopt == 'f' || optopt == 'm' || optopt == 's')
				fprintf(stderr, "-%c option requires an argument\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			ok = B_FALSE;
			break;
		}

		if(!ok)
			break;
	}

	if(!ok || !fflag || (!eflag && !sflag)) {
		if(!fflag)
			fprintf(stderr, "No -f was specified\n");
		if(!eflag && !sflag)
			fprintf(stderr, "-f should be used with -e or -s\n");
		usage();
		exit(1);
	}
}

int main(int argc, const char* argv[]) {
	int ret = 0;

	swat_wl_init();
	swat_map_init();

	parse_options(argc, argv);

	if(swat_read(command == CMD_STAT) != SWAT_OK) {
		ret = 1;
		goto exit;
	}

	if(command == CMD_SER) {
		if(swat_wl_serialize() != SWAT_OK) {
			ret = 1;
			goto exit;
		}
	}

exit:
	swat_map_fini();

	return ret;
}

