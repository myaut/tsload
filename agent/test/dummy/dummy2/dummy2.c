/*
 * dummy2.c
 * should die with SEGV */

int test_main() {
	char *nullptr = (char*) 0;

	*nullptr = 'a';

	return 0;
}
