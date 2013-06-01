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

#define TPNAMELEN		64
#define TPMAXTHREADS 	64

#define TPHASHSIZE		4
#define	TPHASHMASK		3

#define TP_MIN_QUANTUM	10 * T_MS

#define DEFAULT_TP_NAME	"[DEFAULT]"

#define CONTROL_TID		-1
#define WORKER_TID		0

#define	WORKING			0
#define SLEEPING		1
#define INTERRUPT		2

struct request;
struct workload;
struct thread_pool;
struct workload_step;

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

	list_head_t w_requests;

	atomic_t 	w_state;
} tp_worker_t;

typedef struct thread_pool {
	unsigned tp_num_threads;
	char tp_name[TPNAMELEN];

	boolean_t tp_is_dead;

	ts_time_t tp_quantum;			/**< Dispatcher's quantum duration (in ns)*/
	ts_time_t tp_time;				/**< Last time dispatcher had woken up (in ns)*/

	thread_t  tp_ctl_thread;		/**< Dispatcher thread*/
	tp_worker_t* tp_workers;		/**< Worker threads*/

	thread_event_t tp_event;		/**< Used to wake up worker threads when next quantum starts */

	thread_mutex_t tp_mutex;		/**< Protects workload list*/

	list_head_t	   tp_wl_head;
	int tp_wl_count;
	boolean_t tp_wl_changed;

	struct thread_pool* tp_next;
} thread_pool_t;

LIBEXPORT thread_pool_t* tp_create(const char* name, unsigned num_threads, ts_time_t quantum, const char* disp_name);

LIBEXPORT thread_pool_t* tp_search(const char* name);

void tp_attach(thread_pool_t* tp, struct workload* wl);
void tp_detach(thread_pool_t* tp, struct workload* wl);

void tp_distribute_requests(struct workload_step* step, thread_pool_t* tp);

LIBEXPORT int tp_init(void);
LIBEXPORT void tp_fini(void);

#ifndef NO_JSON
#include <libjson.h>

JSONNODE* json_tp_format_all(void);
#endif

#endif /* THREADPOOL_H_ */
