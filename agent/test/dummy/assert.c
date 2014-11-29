/*
 * assert.c - Test that succeds only on April 1st
 *
 * Checks if assert reported correctly to stderr
 */

#include <tsload/defs.h>

#include <assert.h>


int test_main() {
	assert(1 == 0);

	return 0;
}
