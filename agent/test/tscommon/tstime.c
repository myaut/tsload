#include <defs.h>
#include <tstime.h>

#include <assert.h>

/**
 * TS time test.
 *
 * Measures time, sleeps then measures time again.
 * Difference of measurements should be less than epsilon.
 * */
ts_time_t slp_wall = 10 * T_MS;
ts_time_t slp_clock = 100 * T_US;

ts_time_t eps_wall = 100 * T_US;
ts_time_t eps_clock = 100 * T_NS;

void test_time_walltime() {
	ts_time_t start, end;

	start = tm_get_time();
	tm_sleep_milli(slp_wall);
	end = tm_get_time();

	printf("Wall time test: %llu %llu\n", start, end);

	assert(tm_diff(start, end - slp_wall) < eps_wall);
}

void test_time_clock() {
	ts_time_t start, end;

	start = tm_get_clock();
	tm_sleep_nano(slp_clock);
	end = tm_get_clock();

	printf("Clock test: %llu %llu\n", start, end);

	assert(tm_diff(start, end - slp_clock) < eps_clock);
}

int test_main() {
	test_time_walltime();
	test_time_clock();

	return 0;
}
