#include <tsdirent.h>

#include <string.h>
#include <assert.h>

int test_main() {
	plat_dir_t* dir = plat_opendir("test");
	plat_dir_entry_t* de;

	assert(dir != NULL);

	while((de = plat_readdir(dir)) != NULL) {
		if(strcmp(de->d_name, "dir") == 0) {
			assert(plat_dirent_type(de) == DET_DIR);
		}
		else if(strcmp(de->d_name, "file") == 0) {
			assert(plat_dirent_type(de) == DET_REG);
		}
		else {
			plat_dirent_type_t type = plat_dirent_type(de);

			assert(type == DET_CURRENT_DIR || type == DET_PARENT_DIR);
			assert(!plat_dirent_hidden(de));
		}
	}

	assert(plat_closedir(dir) == 0);

	return 0;
}
