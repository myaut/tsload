
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



#ifndef AGENT_H_
#define AGENT_H_

#include <tsload/defs.h>

#include <tsload/errcode.h>

#include <libjson.h>


#define AGENTMAXARGC	16

typedef void (*agent_method_func_t)(JSONNODE* argv[]);

typedef struct {
	char* ad_name;

	struct {
		char* ada_name;
		char ada_type;
	} ad_args[AGENTMAXARGC];

	agent_method_func_t ad_method;
} agent_dispatch_t;

#define AGENT_METHOD(name, args, method)	{	\
	SM_INIT(.ad_name, name),					\
	args,										\
	SM_INIT(.ad_method, method)				}

#define ADT_ARGUMENT(name, type) { SM_INIT(.ada_name, name), \
								   SM_INIT(.ada_type, type) }
#define ADT_ARGS(...)			 { __VA_ARGS__ ADT_ARGUMENT(NULL, JSON_NULL) }

#define ADT_LAST_METHOD() 		 { NULL }

LIBEXPORT void agent_process_command(char* command, JSONNODE* msg);
LIBEXPORT void agent_response_msg(JSONNODE* response);
LIBEXPORT void agent_error_msg(ts_errcode_t code, const char* format, ...);
LIBEXPORT void agent_register_methods(agent_dispatch_t* table);

#define AGENTUUIDLEN 		37
#define AGENTTYPELEN	 	16

LIBIMPORT char agent_uuid[];
LIBIMPORT char agent_type[];

int agent_hello();


#endif /* AGENT_H_ */

