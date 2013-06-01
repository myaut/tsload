/*
 * agent.h
 *
 *  Created on: 29.11.2012
 *      Author: myaut
 */

#ifndef AGENT_H_
#define AGENT_H_

#include <defs.h>
#include <errcode.h>

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

#define AGENTHOSTNAMELEN 	64
#define AGENTTYPELEN	 	16

LIBIMPORT char agent_hostname[];
LIBIMPORT char agent_type[];

int agent_hello();


#endif /* AGENT_H_ */
