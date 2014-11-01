#include <defs.h>
#include <hashmap.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/**
 * Hash map test
 * Tests basic hash map operations for correctness.
 *
 * Does not test:
 * - Concurrencly accessing same hash_map
 * - Hash map internal structure correctness
 * 	 (such as removing first element from head)
 */

struct test {
	char key[32];
	int value;
	struct test* next;
};

#define TEST(aname, avalue)					\
			{ SM_INIT(.key, aname), 		\
			  SM_INIT(.value, avalue), 		\
			  SM_INIT(.next, NULL) }

#define HTESTSIZE	4
#define HTESTMASK	3

DECLARE_HASH_MAP(test_hash_map, B_FALSE, struct test, HTESTSIZE, key, next,
	{
		return hm_string_hash(key, HTESTMASK);
	},
	{
		return strcmp((const char*) key1, (const char*) key2) == 0;
	}
)

struct test elements[] = {
	TEST("banana", 1),
	TEST("orange", 2),
	TEST("lemon", 3),
	TEST("watermelon", 4),
	TEST("apple", 5),
	TEST("pineapple", 6),
	TEST("tomato", 7),
	TEST("potato", 8),
	TEST("cherry", 9)
};
size_t num_elements = sizeof(elements) / sizeof(struct test);

/**
 * Insert tests */
void test_hm_insert_dup() {
	struct test apple = TEST("apple", 100);

	assert(hash_map_insert(&test_hash_map, &apple) == HASH_MAP_DUPLICATE);
}

void test_hm_insert_collision() {
	/* Denends on hm_string_hash implementation that is
	 * currently sum of all character codes.
	 * apple: a->b (+1), l->k (-1)*/
	struct test bppke = TEST("bppke", 100);

	assert(hash_map_insert(&test_hash_map, &bppke) == HASH_MAP_OK);
	assert(hash_map_remove(&test_hash_map, &bppke) == HASH_MAP_OK);
}

/**
 * Find tests */

void test_hm_find_all() {
	int i;
	for(i = 0; i < num_elements ; ++i)
		assert(hash_map_find(&test_hash_map, elements[i].key) == &elements[i]);
}

void test_hm_find_not_exist() {
	/* Nails are not edible, right? */
	assert(hash_map_find(&test_hash_map, "nails") == NULL);
}

/**
 * Remove tests */
void test_hm_remove() {
	/* Remove watermelon */
	assert(hash_map_remove(&test_hash_map, &elements[3]) == HASH_MAP_OK);
}

void test_hm_remove_not_exist() {
	struct test apple = TEST("apple", 100);
	assert(hash_map_remove(&test_hash_map, &apple) == HASH_MAP_NOT_FOUND);
}

/* Print walk test */
struct test_hm_print_walk_state {
	int i;
} print_walk_state;

int test_hm_print_walk_func(hm_item_t* object, void* arg) {
	struct test* t = (struct test*) object;

	assert(arg == &print_walk_state);

	printf("[%d] %s %d\n", print_walk_state.i++, t->key, t->value);

	return HM_WALKER_CONTINUE;
}

void test_hm_print_walk() {
	hash_map_walk(&test_hash_map, test_hm_print_walk_func, &print_walk_state);
}

/* Destroy walk */
int test_hm_destroy_walk_func(hm_item_t* object, void* arg) {
	struct test* t = (struct test*) object;

	assert(t->value > 0 && t->value < 10 && t->value != 4);

	return HM_WALKER_REMOVE | HM_WALKER_CONTINUE;
}

void test_hm_destroy_walk() {
	hash_map_walk(&test_hash_map, test_hm_destroy_walk_func, NULL);
}

int test_main() {
	int i = 0;

	hash_map_init(&test_hash_map, "test_hash_map");

	/* Add elements */
	for(i = 0; i < num_elements; ++i)
		assert(hash_map_insert(&test_hash_map, &elements[i]) == HASH_MAP_OK);

	test_hm_insert_dup();
	test_hm_insert_collision();
	test_hm_find_all();
	test_hm_find_not_exist();
	test_hm_remove();
	test_hm_remove_not_exist();

	test_hm_print_walk();
	test_hm_destroy_walk();

	hash_map_destroy(&test_hash_map);

	return 0;
}
