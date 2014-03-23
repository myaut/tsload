/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>

#include <ilog2.h>
#include <mempool.h>

#include <string.h>
#include <limits.h>

#include <sched.h>
#include <unistd.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <errno.h>

#include <sys/priocntl.h>
#include <sys/tspriocntl.h>
#include <sys/fxpriocntl.h>
#include <sys/fsspriocntl.h>
#include <sys/rtpriocntl.h>

/*
 * Solaris doesn't support affinities like Windows or Linux
 * Instead, it provides processor sets that would exclusively use CPUs.
 *
 * If sched_set_affinity encounters that processor already in pset, it will fail.
 *
 * Also Solaris forbids creating processor sets with all CPUs included, because
 * it that case, it couldn't execute unbound threads anywhere.
 * */

#define DECLARE_UPRI_PARAMS()			\
	DECLARE_SCHED_PARAM("uprilim"),		\
	DECLARE_SCHED_PARAM("upri")

#define DECLARE_TQ_PARAMS()								\
	DECLARE_SCHED_PARAM_STATIC("tqsecs", 0, INT_MAX),	\
	DECLARE_SCHED_PARAM_STATIC("tqnsecs", 0, T_SEC)

#define DECLARE_SOL_SCHED_POLICY(name, os_name)	\
	sched_policy_t sched_ ## name ## _policy = {		\
		os_name, sched_ ## name ## _params, -1	    	\
	};

sched_param_t sched_ts_params[] = {
	DECLARE_UPRI_PARAMS(),
	DECLARE_SCHED_PARAM_END()
};

sched_param_t sched_rt_params[] = {
	DECLARE_SCHED_PARAM("pri"),
	DECLARE_TQ_PARAMS(),
	DECLARE_SCHED_PARAM_END()
};

sched_param_t sched_fss_params[] = {
	DECLARE_UPRI_PARAMS(),
	DECLARE_SCHED_PARAM_END()
};

sched_param_t sched_fx_params[] = {
	DECLARE_UPRI_PARAMS(),
	DECLARE_TQ_PARAMS(),
	DECLARE_SCHED_PARAM_END()
};

DECLARE_SOL_SCHED_POLICY(ts, "TS");
DECLARE_SOL_SCHED_POLICY(rt, "RT");
DECLARE_SOL_SCHED_POLICY(fss, "FSS");
DECLARE_SOL_SCHED_POLICY(fx, "FX");

PLATAPIDECL(sched_get_policies) sched_policy_t* sched_policies[5] = { NULL };

/* Declared internally in libc. Used to implement PC_GETXPARAMS/PC_SETXPARAMS without
 * constructing varargs. Taken from Solaris libc */
extern long __priocntlset(int, procset_t *, int, caddr_t, ...);
static long sched_sol_priocntl(int pc_version, idtype_t idtype, id_t id, int cmd,
							   caddr_t arg, caddr_t arg2)
{
	procset_t	procset;

	setprocset(&procset, POP_AND, idtype, id, P_ALL, 0);
	return (__priocntlset(pc_version, &procset, cmd, arg, arg2));
}

int cpumask_to_pset(psetid_t pset, cpumask_t* mask);

PLATAPI void plat_tsched_init(thread_t* thread) {
	pc_vaparms_t* parms = &thread->t_sched_impl.pcparms;

	thread->t_sched_impl.pset = PS_NONE;
	memset(thread->t_sched_impl.clname, 0, PC_CLNMSZ);
	parms->pc_vaparmscnt = 0;
}

PLATAPI void plat_tsched_destroy(thread_t* thread) {
	if(thread->t_sched_impl.pset != PS_NONE) {
		pset_destroy(thread->t_sched_impl.pset);
	}
}

PLATAPI int sched_get_cpuid(void) {
	return (int) getcpuid();
}

PLATAPI int sched_set_affinity(thread_t* thread , cpumask_t* mask) {
#ifndef HAVE_DECL__LWP_SELF
	/* LWP ID is 0 - set/get affinity is not supported */
	return SCHED_NOT_SUPPORTED;
#else
	int cpuid;
	int ret;

	psetid_t pset;

	if(cpumask_count(mask) == 1) {
		/* For single processor bind, use processor_bind, and processor set
		 * is not constructed */
		cpuid = mask->buckets[0].hibits | ilog2l(mask->buckets[0].mask);
		ret = processor_bind(P_LWPID, thread->t_system_id, cpuid, NULL);

		if(ret == -1)
			return SCHED_ERROR;
	}
	else {
#ifndef HAVE_DECL_PSET_BIND_LWP
		return SCHED_NOT_SUPPORTED;
#else
		ret = pset_create(&pset);

		if(ret == -1) {
			if(errno == ENOTSUP) {
				/* In Solaris 10, psets may be disabled if pools are not configured.  */
				return SCHED_NOT_SUPPORTED;
			}

			return SCHED_ERROR;
		}

		ret = cpumask_to_pset(pset, mask);

		if(ret != SCHED_OK) {
			pset_destroy(pset);
			return ret;
		}

		/* Finally bind a thread */
		pset_bind_lwp(pset, thread->t_system_id,
					  getpid(), NULL);

		if(thread->t_sched_impl.pset != PS_NONE) {
			/* Remove old pset */
			pset_destroy(thread->t_sched_impl.pset);
		}

		thread->t_sched_impl.pset = pset;
#endif
	}

	return SCHED_OK;
#endif
}

PLATAPI int sched_get_affinity(thread_t* thread , cpumask_t* mask) {
#ifndef HAVE_DECL__LWP_SELF
	return SCHED_NOT_SUPPORTED;
#else
	processorid_t* cpulist;
	processorid_t cpu;
	unsigned numcpus;
	int i;

	psetid_t pset = thread->t_sched_impl.pset;

	if(pset == PS_NONE) {
		/* No affinity is set or single processor is used */
		cpumask_reset(mask);

		processor_bind(P_LWPID, thread->t_system_id, PBIND_QUERY, &cpu);
		if(cpu != PBIND_NONE)
			cpumask_set(mask, cpu);

		return SCHED_OK;
	}

	pset_info(pset, NULL, &numcpus, NULL);
	cpulist = mp_malloc(numcpus * sizeof(processorid_t));

	pset_info(pset, NULL, &numcpus, cpulist);

	for(i = 0; i < numcpus; ++i) {
		cpumask_set(mask, cpulist[i]);
	}

	return SCHED_OK;
#endif
}

int cpumask_to_pset(psetid_t pset, cpumask_t* mask) {
	int i;
	char bmask;
	int bid;
	int cpuid;

	psetid_t oldpset = PS_NONE;

	for(bid = 0; bid < mask->num_buckets; ++bid) {
		bmask = mask->buckets[bid].mask;

		for(i = 0; i < 8; ++i) {
			if(bmask & (1 << i)) {
				cpuid = mask->buckets[0].hibits | i;

				pset_assign(PS_QUERY, cpuid, &oldpset);

				/* Already assigned to another pset - return SCHED_NOT_SUPPORTED
				 * because it is design problem of Solaris, not really an error */
				if(oldpset != PS_NONE)
					return SCHED_NOT_SUPPORTED;

				if(pset_assign(pset, cpuid, NULL) == -1)
					return SCHED_ERROR;
			}
		}
	}

	return SCHED_OK;
}

PLATAPI int sched_switch(void) {
	int ret = sched_yield();

	if(ret != 0)
		return SCHED_ERROR;

	return SCHED_OK;
}

#define SCHED_PARAM_KEY(param, key)				\
	if(strcmp(param, name) == 0)				\
		return key

static int sched_sol_param_key(sched_policy_t* policy, const char* name) {
	if(policy == &sched_ts_policy) {
		SCHED_PARAM_KEY("uprilim", TS_KY_UPRILIM);
		SCHED_PARAM_KEY("upri", TS_KY_UPRI);
		return SCHED_INVALID_PARAM;
	}
	if(policy == &sched_rt_policy) {
		SCHED_PARAM_KEY("pri", RT_KY_PRI);
		SCHED_PARAM_KEY("tqsecs", RT_KY_TQSECS);
		SCHED_PARAM_KEY("tqnsecs", RT_KY_TQNSECS);
		return SCHED_INVALID_PARAM;
	}
	if(policy == &sched_fss_policy) {
		SCHED_PARAM_KEY("uprilim", FSS_KY_UPRILIM);
		SCHED_PARAM_KEY("upri", FSS_KY_UPRI);
		return SCHED_INVALID_PARAM;
	}
	if(policy == &sched_fx_policy) {
		SCHED_PARAM_KEY("uprilim", FX_KY_UPRILIM);
		SCHED_PARAM_KEY("upri", FX_KY_UPRI);
		SCHED_PARAM_KEY("tqsecs", FX_KY_TQSECS);
		SCHED_PARAM_KEY("tqnsecs", FX_KY_TQNSECS);
		return SCHED_INVALID_PARAM;
	}

	return SCHED_INVALID_POLICY;
}

static int sched_sol_add_param(pc_vaparms_t* parms, int key, u_longlong_t value) {
	int idx = parms->pc_vaparmscnt++;

	/* Too much parameters supplied - reset params (not policy) and return error */
	if(idx >= PC_VAPARMCNT) {
		parms->pc_vaparmscnt = 0;
		return SCHED_ERROR;
	}

	/* XXX: If param exists - may be we should update it? */

	parms->pc_parms[idx].pc_key = key;
	parms->pc_parms[idx].pc_parm = value;

	return SCHED_OK;
}

static int sched_sol_get_param(pc_vaparms_t* parms, int key, u_longlong_t* value) {
	int idx;

	for(idx = 0; idx < parms->pc_vaparmscnt; ++idx) {
		if(parms->pc_parms[idx].pc_key == key) {
			*value = parms->pc_parms[idx].pc_parm;
			return SCHED_OK;
		}
	}

	if(key == PC_KY_CLNAME)
		return SCHED_INVALID_POLICY;

	return SCHED_INVALID_PARAM;
}

PLATAPI sched_policy_t** sched_get_policies() {
	return sched_policies;
}

PLATAPI int sched_set_policy(thread_t* thread, const char* name) {
	sched_policy_t* policy = sched_policy_find_byname(name);

	if(policy == NULL)
		return SCHED_INVALID_POLICY;

	strncpy(thread->t_sched_impl.clname, name, PC_CLNMSZ);
	return SCHED_OK;
}

PLATAPI int sched_set_param(thread_t* thread, const char* name, int64_t value) {
	pc_vaparms_t* parms = &thread->t_sched_impl.pcparms;

	int err;
	int key;
	sched_policy_t* policy;

	policy = sched_policy_find_byname(thread->t_sched_impl.clname);
	key = sched_sol_param_key(policy, name);
	if(key < 0)
		return key;

	sched_sol_add_param(parms, key, value);
	return SCHED_OK;
}

PLATAPI int sched_commit(thread_t* thread) {
	pc_vaparms_t* parms = &thread->t_sched_impl.pcparms;

	int err;

#ifndef HAVE_DECL__LWP_SELF
	return SCHED_NOT_SUPPORTED;
#else
	parms->pc_parms[parms->pc_vaparmscnt].pc_key = PC_KY_NULL;

	err = sched_sol_priocntl(PC_VERSION, P_LWPID, thread->t_system_id, PC_SETXPARMS,
					 thread->t_sched_impl.clname, parms);

	parms->pc_vaparmscnt = 0;

	if(err != 0)
		return SCHED_ERROR;

	return SCHED_OK;
#endif
}

PLATAPI int sched_get_param(thread_t* thread, const char* name, int64_t* value) {
	pc_vaparms_t parms;
	sched_policy_t* policy;
	pcinfo_t info;
	int err, key;
	char clname[PC_CLNMSZ];

#ifndef HAVE_DECL__LWP_SELF
	return SCHED_NOT_SUPPORTED;
#else
	info.pc_cid = PC_CLNULL;
	err = priocntl(P_LWPID, thread->t_system_id, PC_GETPARMS, &info);
	if(err == -1)
		return SCHED_ERROR;

	policy = sched_policy_find_byid(info.pc_cid);
	if(policy == NULL)
		return SCHED_INVALID_POLICY;

	key = sched_sol_param_key(policy, name);
	if(key < 0)
		return key;

	*value = 0;

	strncpy(clname, policy->name, PC_CLNMSZ);
	parms.pc_vaparmscnt = 1;
	parms.pc_parms[0].pc_key = key;
	parms.pc_parms[0].pc_parm = value;

	err = sched_sol_priocntl(PC_VERSION, P_LWPID, thread->t_system_id, PC_GETXPARMS,
					 	 	 clname, &parms);
	if(err == -1)
		return SCHED_ERROR;
	if(strcmp(clname, policy->name) != 0)
		return SCHED_ERROR;

	return SCHED_OK;
#endif
}

PLATAPI int sched_get_policy(thread_t* thread, char* name, size_t len) {
	sched_policy_t* policy;
	pcinfo_t info;
	int err;

#ifndef HAVE_DECL__LWP_SELF
	return SCHED_NOT_SUPPORTED;
#else
	info.pc_cid = PC_CLNULL;
	err = priocntl(P_LWPID, thread->t_system_id, PC_GETPARMS, &info);
	if(err == -1)
		return SCHED_ERROR;

	policy = sched_policy_find_byid(info.pc_cid);
	if(policy == NULL)
		return SCHED_INVALID_POLICY;

	strncpy(name, policy->name, len);
	return SCHED_OK;
#endif
}

/**
 * Initialize Solaris scheduling class information
 *
 * @param policy pointer to schedutil's policy
 * @param name Solaris scheduler class name
 * @param pri_idx index in parameter array that represents thread's priority
 * @param lim_idx index in parameter array that represents priority limit \
 * 			 	  may be set to -1 if it doesn't supplied for class
 * @param polid last index in sched_policies array
 *
 * @returns 1 if everything went fine or 0, if priocntl failed
 * */
static int sched_sol_init_class(sched_policy_t* policy, const char* name,
								int pri_idx, int lim_idx, int polid) {
	pcinfo_t info;
	int err;
	pri_t maxupri;

	strncpy(info.pc_clname, name, PC_CLNMSZ);

	err = priocntl(0, 0, PC_GETCID, (caddr_t) &info);
	if(err == -1)
		return 0;

	policy->id = info.pc_cid;

	err = priocntl(0, 0, PC_GETCLINFO, (caddr_t) &info);
	if(err == -1)
		return 0;

	/* Since all of four policies (TS, FSS, FX, RT) provide
	 * maximum user priority limit as first integer in class info,
	 * there is no need to cast structures. Simply read maxupri
	 * and copy it to priority-related parameters. */
	maxupri = (pri_t) info.pc_clinfo[0];
	policy->params[pri_idx].max = maxupri;
	if(lim_idx >= 0) {
		policy->params[lim_idx].max = maxupri;
	}

	sched_policies[polid] = policy;

	return 1;
}

PLATAPI int sched_init(void) {
	int polid = 0;

	polid += sched_sol_init_class(&sched_ts_policy, "TS", 1, 0, polid);
	polid += sched_sol_init_class(&sched_rt_policy, "RT", 0, -1, polid);
	polid += sched_sol_init_class(&sched_fss_policy, "FSS", 1, 0, polid);
	polid += sched_sol_init_class(&sched_fx_policy, "FX", 1, 0, polid);
	sched_policies[polid] = NULL;

	return 0;
}

PLATAPI void sched_fini(void) {

}

