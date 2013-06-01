/*
 * main.c
 *
 * Main file for ts-loader daemon
 */


#include <mempool.h>
#include <tsinit.h>
#include <getopt.h>
#include <tsversion.h>

#include <hiprint.h>

#include <hiobject.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int print_flags = INFO_DEFAULT;

struct subsystem subsys[] = {
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("hiobject", hi_obj_init, hi_obj_fini)
};

void usage() {
	fprintf(stderr, "command line: \n"
					"\ttshostinfo [-x] [-l] [{host|cpu|disk|fs|net|all}] [...]\n"
					"\t\t-x - print additional data\n"
					"\t\t-l - print legend\n"
					"\ttshostinfo -v - tsload version\n"
					"\ttshostinfo -h - this help\n");

	exit(1);
}

void parse_options(int argc, char* argv[]) {
	int ok = 1;

	int c;

	while((c = plat_getopt(argc, argv, "vhlx")) != -1) {
		switch(c) {
		case 'v':
			print_ts_version("Host information");
			exit(0);
			break;
		case 'x':
			print_flags |= INFO_XDATA;
			break;
		case 'l':
			print_flags |= INFO_LEGEND;
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			ok = 0;
			break;
		}

		if(!ok)
			break;
	}

	if(!ok) {
		usage();
		exit(1);
	}
}

#define PRINT_INFO_IMPL(_topic)				 						\
	if(print_all || strcmp(topic, #_topic) == 0) {					\
		if(print_##_topic##_info(print_flags) != INFO_OK) {			\
			fprintf(stderr, "Error printing %s info\n", topic);		\
		}															\
		if(!print_all)												\
			return 0;												\
	}

int print_info(const char* topic) {
	boolean_t print_all = TO_BOOLEAN(print_flags & INFO_ALL);

	PRINT_INFO_IMPL(host);
	PRINT_INFO_IMPL(disk);
	PRINT_INFO_IMPL(cpu);

	return 1;
}

int init(void) {
	int count = sizeof(subsys) / sizeof(struct subsystem);
	int i = 0;

	struct subsystem** subsys_list = (struct subsystem**)
			malloc(count * sizeof(struct subsystem*));

	for(i = 0; i < count; ++i ) {
		subsys_list[i] = &subsys[i];
	}

	return ts_init(subsys_list, count);
}

int main(int argc, char* argv[]) {
	int err = 0;
	int i;

	parse_options(argc, argv);
	init();

	if((argc - optind) == 0) {
		print_flags |= INFO_ALL;
		print_info("");
		exit(0);
	}

	for(i = optind; i < argc; ++i) {
		if(print_info(argv[i]) != 0) {
			fprintf(stderr, "Invalid topic %s\n", argv[i]);
			usage();
			exit(1);
		}
	}

	return 0;
}
