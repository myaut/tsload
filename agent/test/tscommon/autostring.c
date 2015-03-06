#include <tsload/defs.h>

#include <tsload/autostring.h>
#include <tsload/mempool.h>

#include <assert.h>
#include <string.h>


void test_aas_copy(void) {
	char* aas;

	assert(aas_copy(aas_init(&aas), "teststring") == 10);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_copy_inval(void) {
	char* aas;

	assert(!AAS_IS_OK(aas_copy(aas_init(&aas), NULL)));
	assert(aas == NULL);
}

void test_aas_merge_1(void) {
	char* aas;

	assert(aas_merge(aas_init(&aas), "teststring", NULL) == 10);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_merge_2(void) {
	char* aas;

	assert(aas_merge(aas_init(&aas), "test", "string", NULL) == 10);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_merge_3(void) {
	char* aas;

	assert(aas_merge(aas_init(&aas), "test", "str", "ing", NULL) == 10);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_merge_inval(void) {
	char* aas;

	assert(!AAS_IS_OK(aas_merge(aas_init(&aas), NULL)));
	assert(aas == NULL);
}

void test_aas_printf(void) {
	char* aas;

	assert(aas_printf(aas_init(&aas), "test%s%03d", "string", 10) == 13);
	assert(strcmp(aas, "teststring010") == 0);

	aas_free(&aas);
}

void test_aas_static(void) {
	char* aas;

	aas_set(aas_init(&aas), "teststring");
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_copyn_less(void) {
	char* aas;

	aas_copy_n(aas_init(&aas), "teststring", 20);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_copyn_exact(void) {
	char* aas;

	aas_copy_n(aas_init(&aas), "teststring", 10);
	assert(strcmp(aas, "teststring") == 0);

	aas_free(&aas);
}

void test_aas_copyn_extra(void) {
	char* aas;

	aas_copy_n(aas_init(&aas), "teststring", 4);
	assert(strcmp(aas, "test") == 0);

	aas_free(&aas);
}

int test_main(void) {
	mempool_init();

	test_aas_copy();
	test_aas_copy_inval();

	test_aas_merge_1();
	test_aas_merge_2();
	test_aas_merge_3();
	test_aas_merge_inval();

	test_aas_printf();

	test_aas_static();
	
	test_aas_copyn_less();
	test_aas_copyn_exact();
	test_aas_copyn_extra();

	mempool_fini();

	return 0;
}
