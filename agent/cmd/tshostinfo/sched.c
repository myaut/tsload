
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

