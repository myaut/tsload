/*
 * workload.h
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#ifndef WORKLOAD_H_
#define WORKLOAD_H_

#include <list.h>

#include <threads.h>
#include <threadpool.h>
#include <wlparam.h>
#include <modules.h>
#include <wltype.h>
#include <tstime.h>

#define WL_NOTIFICATIONS_PER_SEC	20

#define WLHASHSIZE	8
#define WLHASHMASK	(WLHASHSIZE - 1)
#define WLNAMELEN	64

/**
 * Workloads
 * */

/* Keep up to 16 steps in queue */
#define WLSTEPQSIZE	16
#define WLSTEPQMASK	(WLSTEPQSIZE - 1)

/* Request flags sibling of TSRequestFlag */
#define RQF_STARTED		0x01
#define RQF_SUCCESS		0x02
#define RQF_ONTIME		0x04
#define RQF_FINISHED	0x08

struct workload;
struct disp_class;

typedef struct request {
	long rq_step;
	int rq_id;

	int rq_thread_id;

	ts_time_t rq_sched_time;
	ts_time_t rq_start_time;
	ts_time_t rq_end_time;

	int rq_flags;

	struct workload* rq_workload;

	list_node_t rq_node;		/* Next request in chain */
	list_node_t rq_w_node;
	struct request* rq_chain_next;	/* Next request in workload chain */
} request_t;

typedef struct workload_step {
	struct workload* wls_workload;
	unsigned wls_rq_count;
} workload_step_t;

/*Sibling to TSWorkloadStatusCode*/
typedef enum {
	WLS_NEW = 0,
	WLS_CHAINED = 1,
	WLS_CONFIGURING = 2,
	WLS_CFG_FAIL = 3,
	WLS_CONFIGURED = 4,
	WLS_STARTED = 5,
	WLS_RUNNING	= 6,
	WLS_FINISHED = 7,
	WLS_DESTROYED = 8
} wl_status_t;

/**
 * Workload FSM
 *
 * wl_create()  -> NEW / CHAINED
 *                      |
 * cfg_thread()	--> CONFIGURING
 *                     / \
 *                    /   \
 *                   /     \
 *             CONFIGURED CFG_FAIL
 *                 |
 *               STARTED
 *                 |
 *               RUNNING-----+
 *                 |         |
 *              FINISHED     |
 *                 |         |
 *              DESTROYED <--+
 */

typedef struct workload {
	char 			 wl_name[WLNAMELEN];

	wl_type_t*		 wl_type;

	thread_pool_t*	 wl_tp;
	void*			 wl_params;			/**< Provides pointer to parameters structure */
	void*			 wl_private;		/**< Internal data keeped by module */

	thread_t		 wl_cfg_thread;		/**< Thread responsible for configuration*/

	thread_mutex_t	 wl_status_mutex;	/**< Protects wl_status*/
	wl_status_t 	 wl_status;
	unsigned long	 wl_status_flags;

	atomic_t		 wl_ref_count;

	int				 wl_current_rq;

	ts_time_t		 wl_start_time;

	ts_time_t		 wl_notify_time;

	/* Requests queue */
	thread_mutex_t	 wl_rq_mutex;		/**< Mutex that protects wl_requests*/

	long			 wl_current_step;	/**< Id of current step iteration*/
	long			 wl_last_step;		/**< Latest defined step*/

	unsigned		 wl_rqs_per_step[WLSTEPQSIZE];	/**< Contains number of requests per step*/
	/* End of requests queue*/

	struct disp_class* wl_disp_class; /**< Dispatcher class*/
	void* wl_disp_private;			 /**< Dispatcher data */

	struct workload* wl_hm_next;		/**< next in workload hashmap*/
	struct workload* wl_chain_next;		/**< next in workload chain*/

	list_node_t		 wl_tp_node;		/**< thread pool wl list*/
} workload_t;

typedef struct {
	workload_t* wl;
	wl_status_t status;
	long progress;

	char msg[256];
} wl_notify_msg_t;

LIBEXPORT void wl_notify(workload_t* wl, wl_status_t status, long progress, char* format, ...);

LIBEXPORT void wl_destroy(workload_t* wl);

LIBEXPORT workload_t* wl_search(const char* name);

LIBEXPORT void wl_config(workload_t* wl);
LIBEXPORT void wl_unconfig(workload_t* wl);

int wl_is_started(workload_t* wl);
int wl_provide_step(workload_t* wl, long step_id, unsigned num_rqs);
int wl_advance_step(workload_step_t* step);

request_t* wl_create_request(workload_t* wl, request_t* parent);
void wl_run_request(request_t* rq, int thread_id);
void wl_request_free(request_t* rq);
void wl_report_requests(list_head_t* rq_list);

LIBEXPORT int wl_init(void);
LIBEXPORT void wl_fini(void);

/**
 * Workload reference tracker. It may be held by
 * 	- Creator
 * 	- Threadpool it attached to
 * 	- Request belongs to it
 *
 * Last owner released reference frees workload,
 * but not until creator does that.
 * */
void wl_hold(workload_t* wl);
void wl_rele(workload_t* wl);

void wl_finish(workload_t* wl);

#define WL_STEP_OK				0
#define WL_STEP_QUEUE_FULL		-1
#define WL_STEP_INVALID			-2

#ifndef NO_JSON
#include <libjson.h>

LIBEXPORT JSONNODE* json_request_format_all(list_head_t* rq_list);
workload_t* json_workload_proc(const char* wl_name, JSONNODE* node);
#endif

#endif /* WORKLOAD_H_ */
