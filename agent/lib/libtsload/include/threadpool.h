/*
 * threadpool.h
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <defs.h>
#include <list.h>
#include <tstime.h>
#include <threads.h>
#include <syncqueue.h>
#include <atomic.h>
#include <hashmap.h>

#include <stddef.h>

#define TPNAMELEN		64
#define TPMAXTHREADS 	64

#define TPHASHSIZE		4
#define	TPHASHMASK		3

#define TP_MIN_QUANTUM			(10 * T_MS)
#define TP_MAX_QUANTUM			(600 * T_SEC)
#define TP_WORKER_MIN_SLEEP 	(200 * T_US)
#define TP_WORKER_OVERHEAD	 	(30 * T_US)

#define DEFAULT_TP_NAME	"[DEFAULT]"

#define CONTROL_TID		-1
#define WORKER_TID		0

#define TP_ERROR_SCHED_PREFIX	"Failed to schedule threadpool '%s'"

struct request;
struct workload;
struct thread_pool;
struct workload_step;
struct tp_disp;

/**
 * Thread pools are set of threads for executing load requests.
 *
 * It consists of two type of threads:
 * - Control thread which processes step data and distribute requests across workers
 * - Worker thread whose are running requests for execution
 * */

typedef struct tp_worker {
	struct thread_pool* w_tp;
	thread_t w_thread;

    thread_mutex_t w_rq_mutex;
    thread_cv_t w_rq_cv;
    list_head_t	w_rq_head;

	void* w_tpd_data;
} tp_worker_t;

typedef struct thread_pool {
	unsigned tp_num_threads;
	char tp_name[TPNAMELEN];

	boolean_t tp_is_dead;
	boolean_t tp_started;

	ts_time_t tp_quantum;			/**< Dispatcher's quantum duration (in ns)*/
	ts_time_t tp_time;				/**< Last time dispatcher had woken up (in ns)*/

	thread_t  tp_ctl_thread;		/**< Dispatcher thread*/
	tp_worker_t* tp_workers;		/**< Worker threads*/

	thread_mutex_t tp_mutex;				/**< Protects workload list and tp_cv*/
	atomic_t	   tp_ref_count;

	struct tp_disp* tp_disp;
	boolean_t tp_discard;

	list_head_t	   tp_rq_head;

	list_head_t	   tp_wl_head;
	int tp_wl_count;
	boolean_t tp_wl_changed;

	struct thread_pool* tp_next;
} thread_pool_t;

list_head_t* tp_create_worker_list(tp_worker_t* worker);

LIBEXPORT thread_pool_t* tp_create(const char* name, unsigned num_threads,
								   ts_time_t quantum, boolean_t discard,
								   struct tp_disp* disp);
LIBEXPORT void tp_destroy(thread_pool_t* tp);

LIBEXPORT thread_pool_t* tp_search(const char* name);

void tp_attach(thread_pool_t* tp, struct workload* wl);
void tp_detach(thread_pool_t* tp, struct workload* wl);

int tp_compare_requests(struct request* rq1, struct request* rq2);
void tp_insert_request_impl(list_head_t* rq_list, list_node_t* rq_node,
							list_node_t** p_prev_node, list_node_t** p_next_node, ptrdiff_t offset);
#define tp_insert_request(rq_list, rq_node, p_prev_node, p_next_node, member)			\
			tp_insert_request_impl(rq_list, rq_node, p_prev_node, p_next_node,			\
								   offsetof(struct request, member))
void tp_insert_request_initnodes(list_head_t* rq_list, list_node_t** p_prev_node, list_node_t** p_next_node);

void tp_distribute_requests(struct workload_step* step, thread_pool_t* tp);

LIBEXPORT int tp_init(void);
LIBEXPORT void tp_fini(void);

void tp_hold(thread_pool_t* tp);
void tp_rele(thread_pool_t* tp, boolean_t may_destroy);

#ifndef NO_JSON
#include <libjson.h>

JSONNODE* json_tp_format(hm_item_t* object);
int json_tp_schedule(thread_pool_t* tp, JSONNODE* sched);
#endif

#endif /* THREADPOOL_H_ */
