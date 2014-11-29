
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



#ifndef LOADAGENT_H_
#define LOADAGENT_H_

#include <tsload/defs.h>

#include <tsload/list.h>

#include <tsload/agent/agent.h>


void agent_workload_status(const char* wl_name, int status, long progress, const char* config_msg);
void agent_requests_report(list_head_t* rq_list);

LIBEXPORT int agent_init(void);
LIBEXPORT void agent_fini(void);

#endif /* LOADAGENT_H_ */

