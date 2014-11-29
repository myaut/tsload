
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#include <tsload/schedutil.h>

#include <string.h>


#define SCHED_POLICY_FIND(condition)							\
	sched_policy_t** sched_policies = sched_get_policies();		\
	sched_policy_t* policy = sched_policies[0];					\
	int i = 0;													\
	while(policy != NULL) {										\
		if(condition) {											\
			return policy;										\
		}														\
		policy = sched_policies[++i];							\
	}

sched_policy_t* sched_policy_find_byname(const char* name) {
	SCHED_POLICY_FIND(strcmp(name, policy->name) == 0);
	return NULL;
}

sched_policy_t* sched_policy_find_byid(int id) {
	SCHED_POLICY_FIND(id == policy->id);
	return NULL;
}


