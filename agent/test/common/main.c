extern int test_main(int argc, char* argv[]);

extern int fault_init(void);

int main(int argc, char* argv[]) {
#ifndef TEST_NOFAULT
	fault_init();
#endif

	return test_main(argc, argv);
}
