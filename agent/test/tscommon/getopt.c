#include <getopt.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * Test getopt() for correctness
 *
 * On non-POSIX platform libtscommon implements it's own plat_getopt().
 */

const char* testargs[] = {
    "progname",
    "-c",
    "-aftest2abc",
    "-ab",
    "-f", "test3",
    "ARG"
};

int test_main() {
    int c = -1;
    int i = 0;

    while((c = plat_getopt(7, testargs, "f:abc")) != -1) {
    	printf("%d %c %s\n", optind, c,
    				(optarg == NULL) ? "(null)" : optarg);

    	switch(i) {
    	case 0:
    		assert(c == 'c');
    		assert(optind == 2);
    		break;
    	case 1:
			assert(c == 'a');
			assert(optind == 2);
			break;
    	case 2:
			assert(c == 'f' && strcmp(optarg, "test2abc") == 0);
			assert(optind == 3);
			break;
    	case 3:
			assert(c == 'a');
			assert(optind == 3);
			break;
    	case 4:
			assert(c == 'b');
			assert(optind == 4);
			break;
    	case 5:
    		assert(c == 'f' && strcmp(optarg, "test3") == 0);
    		assert(optind == 6);
			break;
    	}

    	++i;
    }

    assert(strcmp(testargs[optind], "ARG") == 0);

    return 0;
}
