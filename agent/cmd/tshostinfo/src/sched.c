/*
 * sched.c
 *
 *  Created on: Feb 1, 2014
 *      Author: myaut
 */

#include <schedutil.h>

#include <hiprint.h>

void print_sched_policy_params(int flags, sched_policy_t* policy) {
	sched_param_t* param = &policy->params[0];

	while(param->name != NULL) {
		printf("\tparam: %s", param->name);

		if(param->min >= 0) {
			printf(" min: %d", param->min);
		}
		if(param->max >= 0) {
			printf(" max: %d", param->max);
		}
		if(param->min == -1 && param->max == -1) {
			fputs(" (get)", stdout);
		}

		fputs("\n", stdout);

		++param;
	}
}

int print_sched_info(int flags) {
	sched_policy_t** policies;
	sched_policy_t* policy;
	int polid = 0;

	print_header(flags, "SCHEDULER POLICIES");

	policies = sched_get_policies();
	policy = policies[0];

	if(flags & INFO_LEGEND)
		printf("%4s %s\n", "ID", "NAME");

	while(policy != NULL) {
		printf("%4d %s\n", policy->id, policy->name);

		if(flags & INFO_XDATA)
			print_sched_policy_params(flags, policy);

		policy = policies[++polid];
	}

	return INFO_OK;
}
