
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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



#ifndef INFO_H_
#define INFO_H_

#include <tsload.h>

#define INFO_XDATA				0x01
#define INFO_HEADER				0x02
#define INFO_LEGEND				0x04

typedef void* (*tsload_walker_func)(tsload_walk_op_t op, void* arg, hm_walker_func walker);
typedef void (*tse_print_func)(void* obj, void* p_flags);

struct tse_info_printer_header {
	int colwidth;
	const char* colhdr;
};

struct tse_info_printer {
	const char* topic;
	const char* alias;
	struct tse_info_printer_header header[8];
	tsload_walker_func walker;	
	tse_print_func printer;
	int flags;
};

void tse_print_wltype(void* obj, void* p_flags);
void tse_print_randgen(void* obj, void* p_flags);
void tse_print_randvar(void* obj, void* p_flags);
void tse_print_tpdisp(void* obj, void* p_flags);
void tse_print_rqsvar(void* obj, void* p_flags);
void tse_print_rqsched(void* obj, void* p_flags);

void tse_print_description(const char* description);
void tse_print_params(int flags, tsload_param_t* params);
int tse_print_walker(hm_item_t* item, void* context);
void tse_print_header(struct tse_info_printer_header* header);


#endif