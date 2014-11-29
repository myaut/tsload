#include <tsload/defs.h>

#include <tsload/pathutil.h>

#include <string.h>
#include <assert.h>


#if defined(PLAT_WIN)
#define ROOT	"C:"
#define PS		"\\"
#define CUR 	"."
#elif defined(PLAT_POSIX)
#define ROOT	""
#define CUR 	"."
#define PS		"/"
#else
#error "Unknown path separator for your platform!"
#endif


const char* path2 = ROOT PS "file";
const char* path3 = ROOT PS "dir" PS "file";
const char* path_2sep = ROOT PS "dir" PS PS "file";
const char* path_cur = ROOT PS "dir" PS CUR PS "file";
const char* path_part = "dir" PS "file";
const char* path_part_inval = "file2";
const char* root = ROOT PS;

void test_path_split_name() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_dirname(&iter, path2)) == 0);
	assert(strcmp("file", path_basename(&iter, path2)) == 0);
}

void test_path_split_1() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_split(&iter, 8, root)) == 0);
	assert(strcmp("file", path_split(&iter, -8, "file")) == 0);
}

void test_path_split_2() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_split(&iter, 8, path2)) == 0);
	assert(strcmp("file", path_split_next(&iter)) == 0);
	assert(path_split_next(&iter) == NULL);
}

void test_path_split_3() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_split(&iter, 8, path3)) == 0);
	assert(strcmp("dir", path_split_next(&iter)) == 0);
	assert(strcmp("file", path_split_next(&iter)) == 0);
	assert(path_split_next(&iter) == NULL);
}

void test_path_split_3_rev() {
	path_split_iter_t iter;

	assert(strcmp("file", path_split(&iter, -8, path3)) == 0);
	assert(strcmp("dir", path_split_next(&iter)) == 0);
	assert(strcmp(ROOT, path_split_next(&iter)) == 0);
	assert(path_split_next(&iter) == NULL);
}

void test_path_split_2sep() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_split(&iter, 8, path_2sep)) == 0);
	assert(strcmp("dir", path_split_next(&iter)) == 0);
	assert(strcmp("file", path_split_next(&iter)) == 0);
	assert(path_split_next(&iter) == NULL);
}

void test_path_split_cur() {
	path_split_iter_t iter;

	assert(strcmp(ROOT, path_split(&iter, 8, path_cur)) == 0);
	assert(strcmp("dir", path_split_next(&iter)) == 0);
	assert(strcmp("file", path_split_next(&iter)) == 0);
	assert(path_split_next(&iter) == NULL);
}

void test_path_join() {
	char dst[64];

	assert(strcmp(path_join(dst, 64, ROOT, "file", NULL), path2) == 0);
	assert(strcmp(path_join(dst, 64, ROOT, "dir", "file", NULL), path3) == 0);
}

void test_path_join_array() {
	char dst[64];
	const char* parts2[] = {ROOT, "file"};
	const char* parts3[] = {ROOT, "dir", "file"};

	assert(strcmp(path_join_array(dst, 64, 2, parts2), path2) == 0);
	assert(strcmp(path_join_array(dst, 64, 3, parts3), path3) == 0);
}

void test_path_remove() {
	char dst[64];

	assert(strcmp(path_remove(dst, 64, path_cur, path_part), ROOT) == 0);
	assert(path_remove(dst, 64, path_cur, path_part_inval) ==  NULL);
}

int test_main() {
	test_path_split_1();
	test_path_split_2();
	test_path_split_3();
	test_path_split_3_rev();
	test_path_split_cur();
	test_path_split_name();

	test_path_join();
	test_path_join_array();

	test_path_remove();

	return 0;
}
