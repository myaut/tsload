
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



#ifndef SWAT_H_
#define SWAT_H_

#include <tsload/defs.h>

#include <tsload/time.h>
#include <tsload/list.h>

#include <stdint.h>


#define SWATWLNAMELEN		32

#define SWATCHUNKLEN		256

#define SWATWLHASHSIZE		16
#define SWATWLHASHMASK		(SWATWLHASHSIZE - 1)

typedef struct swat_record {
	int64_t sr_start;
	int64_t sr_resp;
	int64_t sr_device;
	int64_t sr_lba;

	int32_t sr_xfersize;
	int32_t sr_pid;

	int64_t sr_flag;
} swat_record_t;

typedef struct {
	int64_t		swl_device;
	int32_t 	swl_xfersize;
	int64_t 	swl_flag;
} swl_key_t;

typedef struct {
	unsigned swl_num_rqs[SWATCHUNKLEN];

	list_node_t swl_node;
} swl_chunk_t;

typedef struct swat_workload {
	swl_key_t swl_key;

	long 	  swl_last_step;
	unsigned long  swl_total_rqs;

	list_head_t swl_num_rqs;

	swl_chunk_t* swl_last_chunk;
	struct swat_workload* swl_next;

	char swl_name[SWATWLNAMELEN];
} swat_workload_t;

#define SWAT_ERR_SERIALIZE 	-2

typedef enum { CMD_SER, CMD_STAT } swat_command_t;

int swat_wl_init(void);
int swat_wl_serialize(void);

#endif /* SWAT_H_ */

