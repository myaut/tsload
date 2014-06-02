
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#include <defs.h>

#include <log.h>
#include <mempool.h>
#include <tstime.h>
#include <threads.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MALLOCCOUNT 		1000
#define MALLOCMAXSIZE		512
#define MALLOCTHREADS		4

LIBIMPORT char log_filename[LOGFNMAXLEN];

#define CLOCK_DIFF(t2, t1)  (((double) (t2 - t1)) / T_SEC)

int mallocs[MALLOCCOUNT];

struct benchmark {
	void* (*alloc_func)(size_t sz);
	void (*free_func)(void* ptr);

	double tm_malloc;
	double tm_free;
	double tm_2nd_malloc;
};

thread_event_t bench_event;

thread_result_t bench(thread_arg_t arg) {
	THREAD_ENTRY(arg, struct benchmark, b);

	int i;
	void* ptrs[MALLOCCOUNT];
	ts_time_t t1, t2, t3, t4;

	event_wait(&bench_event);

	t1 = tm_get_time();

	for(i = 0; i < (MALLOCCOUNT / 2); ++i) {
		ptrs[i] = b->alloc_func(mallocs[i]);
	}

	t2 = tm_get_time();

	for(i = 0; i < (MALLOCCOUNT / 2); i += 2) {
		b->free_func(ptrs[i]);
	}

	t3 = tm_get_time();

	for(i = (MALLOCCOUNT / 2); i < MALLOCCOUNT; ++i) {
		ptrs[i] = b->alloc_func(mallocs[i]);
	}

	t4 = tm_get_time();

	for(i = 1; i < (MALLOCCOUNT / 2); i += 2)
		b->free_func(ptrs[i]);
	for(i = (MALLOCCOUNT / 2); i < MALLOCCOUNT; ++i)
		b->free_func(ptrs[i]);

	b->tm_malloc 	 =  CLOCK_DIFF(t2, t1);
	b->tm_free 		 =  CLOCK_DIFF(t3, t2);
	b->tm_2nd_malloc =  CLOCK_DIFF(t4, t3);
THREAD_END:
	THREAD_FINISH(arg);
}

int run_bechmark(const char* name,
				 void* (*alloc_func)(size_t sz),
				 void (*free_func)(void* ptr)) {
	thread_t bench_threads[MALLOCTHREADS];
	int j = 0;

	struct benchmark b[MALLOCTHREADS];

	for(j = 0; j < MALLOCTHREADS; ++j) {
		b[j].alloc_func = alloc_func;
		b[j].free_func  = free_func;

		t_init(bench_threads + j, &b[j], bench, "bench-%d", j);
		t_wait_start(bench_threads + j);
	}

	event_notify_all(&bench_event);

	printf("%s results:\n", name);

	for(j = 0; j < MALLOCTHREADS; ++j) {
		t_destroy(bench_threads + j);

		printf("\t %d	%.12f %.12f %.12f\n", j, b[j].tm_malloc,
				 b[j].tm_free, b[j].tm_2nd_malloc);
	}
}

int main() {
	int i;

	strcpy(log_filename, "-");

	log_init();
	mempool_init();
	threads_init();

	event_init(&bench_event, "bench_event");

	printf("%d mallocs, %d frees, %d mallocs \n", MALLOCCOUNT / 2,
			MALLOCCOUNT / 4, MALLOCCOUNT / 2);

	run_bechmark("libc", malloc, free);
	run_bechmark("mempool", mp_malloc, mp_free);

	threads_fini();
	mempool_fini();
	log_fini();

	return 0;
}

