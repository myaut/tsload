
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

#include <tsload/pathutil.h>

#include <swat.h>
#include <swatfile.h>

#include <stdio.h>

#include <zlib.h>


char flat_filename[PATHMAXLEN];

boolean_t conv_endianess = B_FALSE;

long report_threshold = 5 * SZ_MB;

#ifdef SWAT_TRACE
boolean_t swat_trace = B_TRUE;
#endif

gzFile* flat_gz;

void swat_record_endianess(swat_record_t* sr) {
	sr->sr_start = SWAT_64_ENDIANESS(sr->sr_start);
	sr->sr_resp = SWAT_64_ENDIANESS(sr->sr_resp);
	sr->sr_device = SWAT_64_ENDIANESS(sr->sr_device);
	sr->sr_lba = SWAT_64_ENDIANESS(sr->sr_lba);

	sr->sr_xfersize = SWAT_32_ENDIANESS(sr->sr_xfersize);
	sr->sr_pid = SWAT_32_ENDIANESS(sr->sr_pid);

	sr->sr_flag = SWAT_64_ENDIANESS(sr->sr_flag);
}

int swat_read(boolean_t do_report) {
	uint64_t control;
	swat_record_t sr;
	unsigned length, type, version;

	long last_off = 0, cur_off;
	unsigned long entry_count = 0;

	flat_gz = gzopen(flat_filename, "r");

	if(flat_gz == NULL) {
		fprintf(stderr, "Couldn't open file %s\n", flat_filename);
		return SWAT_ERR_OPEN_FILE;
	}

	/* printf("Processing swatfile... "); */

	while(gzread(flat_gz, &control, SWAT_LONG_SIZE) > 0) {
		if(conv_endianess)
			control = SWAT_64_ENDIANESS(control);

		length = control & 0xFFFFFF;
		type = control >> 40 & 0xFF;
		version = control >> 32 & 0xFF;

		if(type != SWAT_FLAT_RECORD) {
			gzseek(flat_gz, length * SWAT_LONG_SIZE, SEEK_CUR);
			continue;
		}

		gzread(flat_gz, &sr, sizeof(swat_record_t));

		if(conv_endianess)
			swat_record_endianess(&sr);

#		ifdef SWAT_TRACE
		if(swat_trace)
			printf("%22lld %22lld %15ld %1lld\n", sr.sr_start, sr.sr_device, sr.sr_xfersize, sr.sr_flag);
#		endif

		swat_add_entry(&sr);
		++entry_count;

		if(do_report) {
			/* Report */
			cur_off = gztell(flat_gz);

			if((cur_off - last_off) > report_threshold) {
				printf("%8ldM ", cur_off / SZ_MB);
				fflush(stdout);
				last_off = cur_off;
			}
		}
	}

	if(do_report)
		printf("\nProcessed %ld entries\n", entry_count);

	gzclose(flat_gz);

	return SWAT_OK;
}

