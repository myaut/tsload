#include <ilog2.h>

#include <assert.h>

int test_main() {
	assert(ilog2l(0x80000000) == 31);
	assert(ilog2l(0xffffffff) == 32);

	assert(ilog2l(0x10) == 4);
	assert(ilog2l(0x11) == 5);

	assert(ilog2l(1) == 0);
	assert(ilog2l(0) == -1);

	assert(ilog2ll(0x8000000000000000) == 63);
	assert(ilog2ll(0xffffffffffffffff) == 64);
	assert(ilog2ll(0x100000000) == 32);

	return 0;
}


