#include <defs.h>
#include <tstime.h>
#include <client.h>
#include <agent.h>
#include <log.h>
#include <mempool.h>
#include <tsinit.h>

#include <libjson.h>

#include <stdlib.h>
#include <stdio.h>

#define ITERATIONS	 500

LIBIMPORT int log_debug;
LIBIMPORT int log_trace;

LIBIMPORT char log_filename[];

extern boolean_t clnt_connected;

void agent_test_invoke() {
	JSONNODE *response;
	JSONNODE *msg = json_new(JSON_NODE);

	clnt_response_type_t rtype;

	rtype = clnt_invoke("test_invoke", msg, &response);

	if(rtype == RT_RESPONSE || rtype == RT_ERROR)
		json_delete(response);
}

void agent_test_call(JSONNODE* argv[]) {
	agent_response_msg(json_new(JSON_NODE));
}

static agent_dispatch_t bench_table[] = {
	AGENT_METHOD("test_call",
		ADT_ARGS(),
		agent_test_call),
};

struct subsystem subsys[] = {
	SUBSYSTEM("log", log_init, log_fini),
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("threads", threads_init, threads_fini),
	SUBSYSTEM("client", clnt_init, clnt_fini),
};

int init(void) {
	int count = sizeof(subsys) / sizeof(struct subsystem);
	int i = 0;

	struct subsystem** subsys_list = (struct subsystem**)
			malloc(count * sizeof(struct subsystem*));

	for(i = 0; i < count; ++i ) {
		subsys_list[i] = &subsys[i];
	}

	return ts_init(subsys_list, count);
}


int main(int argc, char* argv[]) {
	int i = 0;
	ts_time_t begin, end;

	double iter_time;

	if(argc == 1) {
		fprintf(stderr, "Usage: jsontsbench <hostname>\n");
		exit(1);
	}

	strncpy(log_filename, "-", LOGFNMAXLEN);
	log_debug = 1;
	log_trace = 1;

	strncpy(agent_hostname, argv[1], AGENTHOSTNAMELEN);
	agent_register_methods(bench_table);

	init();

	do {
		tm_sleep_milli(200 * T_MS);
	} while(!clnt_connected);

	begin = tm_get_clock();
	for(i = 0; i < ITERATIONS; ++i) {
		agent_test_invoke();
	}
	end = tm_get_clock();

	iter_time = ((double) (end - begin) / ITERATIONS);

	printf("Report %s:\n", argv[1]);
	printf("%f iterations/sec\n", T_SEC / iter_time);
	printf("%f ms per iteration\n", iter_time / T_MS);

	return 0;
}
