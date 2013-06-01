extern int test_main(int argc, char* argv[]);

extern int fault_init(void);

int main(int argc, char* argv[]) {
	fault_init();

	return test_main(argc, argv);
}
