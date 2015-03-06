#include <tsload/defs.h>

#include <tsload/time.h>
#include <tsload/threads.h>

#include <assert.h>


#define TEST_THREAD_MAGIC		0x146ead

thread_t t;

thread_result_t test_thread_1(thread_arg_t arg) {
	THREAD_ENTRY(arg, int, pmagic);

	/* Check that correct parameter was passed */
	assert(*pmagic == TEST_THREAD_MAGIC);

	/* Check that t_self()/thread returns valid value */
	assert(t_self() == &t);
	assert(thread == &t);

	tm_sleep_milli(100 * T_MS);

THREAD_END:
	THREAD_FINISH(arg);
}


int test_main() {
	int magic = TEST_THREAD_MAGIC;

	threads_init();

	t_init(&t, (void*) &magic, test_thread_1, "test_thread_1");
	t_join(&t);

	/* After t_join thread should be dead */
	assert(t.t_state == TS_DEAD);

	t_destroy(&t);

	threads_fini();

	return 0;
}
