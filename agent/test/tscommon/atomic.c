#include <atomic.h>

#include <assert.h>

/* Checks atomic operations
 *
 * NOTE: Only checks arithmetical correctness,
 * atomic writes are checked by threads/atomic test */

#define MAGIC1			0xbaadcafe
#define MAGIC2			0xdeadbeaf

#define ATOM1			0x100500

void test_atomic_set() {
	atomic_t atom;

	atomic_set(&atom, MAGIC1);
	assert(atomic_read(&atom) == MAGIC1);
}

void test_atomic_exchange() {
	atomic_t atom = MAGIC1;

	assert(atomic_exchange(&atom, MAGIC2) == MAGIC1);
}

void test_atomic_incdec() {
	atomic_t atom = ATOM1;

	assert(atomic_inc(&atom) == ATOM1);
	assert(atomic_dec(&atom) == (ATOM1 + 1));
	assert(atomic_read(&atom) == ATOM1);
}

void test_atomic_addsub() {
	atomic_t atom = ATOM1;

	assert(atomic_add(&atom, 20) == ATOM1);
	assert(atomic_sub(&atom, 20) == (ATOM1 + 20));
	assert(atomic_read(&atom) == ATOM1);
}

void test_atomic_bitwise() {
	atomic_t atom = ATOM1;

	assert(atomic_or(&atom, 0xff) == ATOM1);
	assert(atomic_and(&atom, 0xffffff00) == (ATOM1 | 0xff));
	assert(atomic_read(&atom) == ATOM1);
}

int test_main() {
	test_atomic_set();
	test_atomic_exchange();
	test_atomic_incdec();
	test_atomic_addsub();
	test_atomic_bitwise();

	return 0;
}
