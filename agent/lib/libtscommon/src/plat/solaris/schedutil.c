/*
 * schedutil.c
 *
 *  Created on: 16.06.2013
 *      Author: myaut
 */

#include <schedutil.h>

#include <ilog2.h>
#include <mempool.h>

#include <sched.h>
#include <unistd.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <errno.h>

/*
 * Solaris doesn't support affinities like Windows or Linux
 * Instead, it provides processor sets that would exclusively use CPUs.
 *
 * If sched_set_affinity encounters that processor already in pset, it will fail.
 *
 * Also Solaris forbids creating processor sets with all CPUs included, because
 * it that case, it couldn't execute unbound threads anywhere.
 * */

int cpumask_to_pset(psetid_t pset, cpumask_t* mask);

PLATAPI void plat_tsched_init(thread_t* thread) {
	thread->t_sched_impl.pset = PS_NONE;
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
