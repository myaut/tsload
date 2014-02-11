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
#include <randgen.h>

#define WL_NOTIFICATIONS_PER_SEC	20

#define WLHASHSIZE	8
#define WLHASHMASK	(WLHASHSIZE - 1)
#define WLNAMELEN	64

#define WLNOTIFYMSGLEN	512

/**
 * Workloads
 * */

/* Keep up to 16 steps in queue */
#define WLSTEPQSIZE	16
#define WLSTEPQMASK	(WLSTEPQSIZE - 1)

/**
 * Request flags.
 * sibling of TSRequestFlag */
#define RQF_STARTED		0x0001
#define RQF_SUCCESS		0x0002
#define RQF_ONTIME		0x0004
#define RQF_FINISHED	0x0008

/* Internal flags for TP dispatcher */
#define RQF_DISPATCHED	0x0100
#define RQF_DEQUEUED	0x0200
#define RQF_TRACE		0x0400

#define RQF_FLAG_MASK	0x00ff

struct workload;
struct rqsched_class;

typedef struct request {
	long rq_step;
	int rq_id;
	int rq_user_id;

	int rq_thread_id;

	ts_time_t rq_sched_time;
	ts_time_t rq_start_time;
	ts_time_t rq_end_time;

	int rq_flags;

	int rq_queue_len;

	struct workload* rq_workload;

	void* rq_params;

	list_node_t rq_node;		/* Next request in chain */
	list_node_t rq_w_node;
	struct request* rq_wl_next;
	struct request* rq_chain_next;	/* Next request in workload chain */
} request_t;

typedef struct workload_step {
	struct workload* wls_workload;
	unsigned wls_rq_count;
	list_head_t wls_trace_rqs;
} workload_step_t;

/**
 * Workload state
 *
 * Sibling to TSWorkloadStatusCode*/
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
 * Workload primary structure
 *
 * Workload FSM
 *
 * ```
 * wl_create()  -> NEW / CHAINED
 *                      |
 * cfg_thread()	--> CONFIGURING
 *                     / \
 *                    /   \
 *                   /     \
 *             CONFIGURED CFG_FAIL
 *                 |		 |
 *               STARTED	 |
 *                 |		 |
 *               RUNNING-----+
 *                 |         |
 *              FINISHED     |
 *                 |         |
 *              DESTROYED <--+
 * ```
 *
 * @member wl_name 	Name of workload. Length limited to WLNAMELEN
 * @member wl_type	Reference to workload type descriptor, provided by module
 * @member wl_tp Reference to thread pool it was attached. May be set to NULL for chained workloads
 * @member wl_params Vector of workload parameters
 * @member wl_private Private field to keep module-specific data
 * @member wl_cfg_thread Thread responsible for configuration
 * @member wl_status Current workload state (see FSM above)
 * @member wl_status_flags Bitmap that contains workload states it was passed. Use WL_HAD_FLAGS to check it
 * @member wl_ref_count Reference counter. Workload is referenced by creator and requests			\
 * 		Because after we unconfiguring workload there are requests that was not yet reported 		\
 * 		and it is done by separate thread, we should wait for them.
 * @member wl_current_rq Id of last created request
 * @member wl_last_request Most newer request created for this workload
 * @member wl_start_time Time when workload was scheduled to start
 * @member wl_notify_time Timestamp when wl_notify was called. Used to reduce number of WLS_CONFIGURING messages
 * @member wl_start_clock Clock when workload was run by threadpool. Used to normalize request times to	\
 * 		start time of workload
 * @member wl_time Current clock of workload: wl_start_clock + step * tp_quantum
 * @member wl_current_step Current step of workload
 * @member wl_last_step Last step id on queue
 * @member wl_step_queue Queue that contains requests number of requests (or trace-based)
 * @member wl_rqsched_class Workload request scheduler
 * @member wl_rqsched_private Private data for request scheduler
 */
typedef struct workload {
	char 			 wl_name[WLNAMELEN];

	wl_type_t*		 wl_type;

	thread_pool_t*	 wl_tp;
	void*			 wl_params;
	void*			 wl_private;

	thread_t		 wl_cfg_thread;

	thread_mutex_t	 wl_status_mutex;
	wl_status_t 	 wl_status;
	unsigned long	 wl_status_flags;

	atomic_t		 wl_ref_count;

	int				 wl_current_rq;
	request_t*		 wl_last_request;

	ts_time_t		 wl_start_time;
	ts_time_t		 wl_notify_time;
	ts_time_t		 wl_start_clock;
	ts_time_t		 wl_time;

	/* Requests queue */
	thread_mutex_t	 wl_rq_mutex;
	long			 wl_current_step;
	long			 wl_last_step;
	workload_step_t  wl_step_queue[WLSTEPQSIZE];
	/* End of requests queue*/

	ts_time_t		 wl_deadline;

	struct workload* wl_chain_next;
	randgen_t*		 wl_chain_rg;
	double			 wl_chain_probability;

	struct rqsched_class* wl_rqsched_class;
	void* wl_rqsched_private;

	struct workload* wl_hm_next;		/**< next in workload hashmap*/

	list_node_t		 wl_tp_node;		/**< thread pool wl list*/

	list_head_t		 wl_wlpgen_head;	/**< per-request params*/
} workload_t;

typedef struct {
	char wl_name[WLNAMELEN];
	wl_status_t status;
	long progress;

	char msg[WLNOTIFYMSGLEN];
} wl_notify_msg_t;

LIBEXPORT void wl_notify(workload_t* wl, wl_status_t status, long progress, char* format, ...);

LIBEXPORT void wl_destroy(workload_t* wl);

LIBEXPORT workload_t* wl_search(const char* name);

LIBEXPORT void wl_config(workload_t* wl);
LIBEXPORT void wl_unconfig(workload_t* wl);

int wl_is_started(workload_t* wl);
int wl_provide_step(workload_t* wl, long step_id, unsigned num_rqs, list_head_t* trace_rqs);
workload_step_t* wl_advance_step(workload_t* wl);

request_t* wl_create_request(workload_t* wl, request_t* parent);
request_t* wl_clone_request(request_t* origin);
request_t* wl_create_request_trace(workload_t* wl, int rq_id, long step, int user_id, int thread_id,
								   ts_time_t sched_time, void* rq_params);

void wl_run_request(request_t* rq);
void wl_request_free(request_t* rq);
void wl_report_requests(list_head_t* rq_list);

void wl_destroy_request_list(list_head_t* rq_list);

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
workload_t* json_workload_proc(const char* wl_name, const char* wl_type, const char* tp_name, ts_time_t deadline,
		                       JSONNODE* wl_chain_params, JSONNODE* rqsched_params, JSONNODE* wl_params);
#endif

#endif /* WORKLOAD_H_ */
