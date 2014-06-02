
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



#define LOG_SOURCE "agent"
#include <log.h>

#include <agent.h>
#include <client.h>

#include <libjson.h>

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

static agent_dispatch_t* agent_dispatch_table = NULL;

/* Destination agent id.
 *
 * Set to root agent id to send hello message.
 *
 * After that - sets to our agentId and listener flows are used by
 * server and experiment services */
int agent_id = 0;

char agent_uuid[AGENTUUIDLEN];
char agent_type[AGENTTYPELEN];

void agent_register_methods(agent_dispatch_t* table) {
	agent_dispatch_table = table;
}

JSONNODE* agent_hello_msg() {
	JSONNODE* node = json_new(JSON_NODE);

	json_push_back(node, json_new_a("agentUuid", agent_uuid));
	json_push_back(node, json_new_a("agentType", agent_type));

	return node;
}

void agent_response_msg(JSONNODE* response) {
	JSONNODE* node = json_new(JSON_NODE);

	assert(response != NULL);

	json_set_name(response, "response");
	json_push_back(node, response);

	clnt_add_response(RT_RESPONSE, node);
}

void agent_error_msg(ts_errcode_t code, const char* format, ...) {
	JSONNODE* node = json_new(JSON_NODE);
	char error_msg[256];
	va_list args;

	clnt_proc_msg_t* msg = clnt_proc_get_msg();

	va_start(args, format);
	vsnprintf(error_msg, 256, format, args);
	va_end(args);

	json_push_back(node, json_new_i("code", code));
	json_push_back(node, json_new_a("error", error_msg));

	logmsg(LOG_WARN, "Error while processing command %s msg #%u: %s",
			msg->m_command, msg->m_msg_id, error_msg);

	clnt_add_response(RT_ERROR, node);
}

void agent_process_command(char* command, JSONNODE* n_msg) {
	agent_dispatch_t* method = agent_dispatch_table;
	int i = 0, num_args = 0;

	char* arg_name;
	char arg_type;

	JSONNODE* argv[AGENTMAXARGC];
	JSONNODE* n_arg;

	assert(agent_dispatch_table != NULL);

	while(method->ad_name) {
		if(strcmp(method->ad_name, command) != 0) {
			++method;
			continue;
		}

		/* Process arguments. Incoming n_msg is JSON_NODE with
		 * subnodes which names are matching arguments names.*/
		for(i = 0; i < AGENTMAXARGC; ++i) {
			arg_name = method->ad_args[i].ada_name;
			arg_type = method->ad_args[i].ada_type;

			if(arg_name == NULL) {
				num_args = i + 1;
				break;
			}

			n_arg = json_get(n_msg, arg_name);

			if(n_arg == NULL) {
				return agent_error_msg(TSE_MESSAGE_FORMAT, "Missing argument %s!", arg_name);
			}

			if(arg_type != JSON_NULL && json_type(n_arg) != arg_type) {
				return agent_error_msg(TSE_MESSAGE_FORMAT, "Invalid type of argument %s!", arg_name);
			}

			argv[i] = n_arg;
		}

		method->ad_method(argv);
		return;
	}

	agent_error_msg(TSE_COMMAND_NOT_FOUND, "Not found command %s!", command);
}

int agent_hello() {
	int ret = -1;
	JSONNODE* response;
	clnt_response_type_t rtype;

	JSONNODE_ITERATOR i_end, i_agentid;

	rtype = clnt_invoke("hello", agent_hello_msg(), &response);

	if(rtype == RT_RESPONSE) {
		i_end = json_end(response);
		i_agentid = json_find(response, "agentId");

		if(i_agentid == i_end || json_type(*i_agentid) != JSON_NUMBER) {
			logmsg(LOG_CRIT, "Failed to process hello response: unknown 'agentId'");
			ret = -1;
			goto fail;
		}

		agent_id = json_as_int(*i_agentid);

		ret = 0;
	}
	else {
		logmsg(LOG_CRIT, "Error or timeout after hello command");
		ret = -1;
	}

fail:
	return ret;
}

