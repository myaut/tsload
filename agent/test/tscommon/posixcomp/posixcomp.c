#include <tsload/defs.h>

#include <tsload/posixdecl.h>

#include <stdio.h>


/**
 * Test POSIX compability interfaces:
 * - open() /close ()
 * - access()
 * */

int test_main() {
	int fd;

	if(access("file", R_OK | W_OK) != 0) {
		perror("access");
		return 1;
	}

	if((fd = open("file", O_RDWR)) == -1) {
		perror("open");
		return 1;
	}

	if(open("not_exists", O_RDONLY) != -1) {
		perror("open (not_exists)");
		return 1;
	}

	close(fd);

	return 0;
}
