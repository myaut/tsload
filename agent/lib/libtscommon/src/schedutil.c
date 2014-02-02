/*
 * schedutil.c
 *
 *  Created on: Jan 28, 2014
 *      Author: myaut
 */

#include <schedutil.h>

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

