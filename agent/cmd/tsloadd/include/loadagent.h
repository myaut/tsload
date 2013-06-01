/*
 * loadagent.h
 *
 *  Created on: 30.11.2012
 *      Author: myaut
 */

#ifndef LOADAGENT_H_
#define LOADAGENT_H_

#include <list.h>
#include <agent.h>

void agent_workload_status(const char* wl_name, int status, long progress, const char* config_msg);
void agent_requests_report(list_head_t* rq_list);

LIBEXPORT int agent_init(void);
LIBEXPORT void agent_fini(void);

#endif /* LOADAGENT_H_ */
