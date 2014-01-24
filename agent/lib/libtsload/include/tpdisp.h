#ifndef TPDISP_H
#define TPDISP_H

#include <workload.h>
#include <threadpool.h>

struct tp_disp;

#define TPD_OK				0
#define TPD_ERROR			-1

typedef struct tp_disp_class {
	int (*init)(thread_pool_t* tp);
	void (*destroy)(thread_pool_t* tp);

	void (*control_report)(thread_pool_t* tp);
	void (*control_sleep)(thread_pool_t* tp);

	request_t* (*worker_pick)(thread_pool_t* tp, tp_worker_t* worker);
	void (*worker_done)(thread_pool_t* tp, tp_worker_t* worker, request_t* rq);
	void (*worker_signal)(thread_pool_t* tp, int wid);
} tp_disp_class_t;

typedef struct tp_disp {
	thread_pool_t*	tpd_tp;
	tp_disp_class_t* tpd_class;
	void* tpd_data;
} tp_disp_t;

#ifndef NO_JSON
#include <libjson.h>

LIBEXPORT tp_disp_t* json_tp_disp_proc(JSONNODE* node);
#endif

LIBEXPORT void tpd_destroy(tp_disp_t* tpd);

#endif
