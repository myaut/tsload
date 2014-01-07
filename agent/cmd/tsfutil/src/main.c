/*
 * main.c
 *
 * Main file for ts-loader daemon
 */

#define LOG_SOURCE "tsfutil"
#include <log.h>

#include <mempool.h>
#include <getopt.h>
#include <tsversion.h>
#include <pathutil.h>
#include <tsinit.h>

#include <tsfile.h>
#include <tsfutil.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LIBIMPORT char log_filename[];
LIBIMPORT int log_debug;
LIBIMPORT int log_trace;

tsfutil_backend_t* backend = &json_backend;
int command = COMMAND_CREATE;

char tsf_path[PATHMAXLEN];
char schema_path[PATHMAXLEN];

int start = 0;
int end = -1;

char file_path[PATHMAXLEN];
boolean_t use_std_streams = B_FALSE;

LIBEXPORT struct subsystem subsys[] = {
	SUBSYSTEM("log", log_init, log_fini),
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("threads", threads_init, threads_fini),
	SUBSYSTEM("tsfile", tsfile_init, tsfile_fini)
};

void usage() {
	fprintf(stderr, "command line: \n"
					"\ttsgenuuid -s <schema.json> [-F <csv|json|jsonraw>] [action] <tsf-file> [<file>] \n"
					"\t\tWhere action is one of:\n"
					"\t\t  (no option) - create new TSF\n"
					"\t\t  -a - add entries from <file>\n"
					"\t\t  -c - get entries count \n"
					"\t\t  -g 0:$ - get entries range. $ means last entry. \n"
					"\t\tIf file is omitted, then stdin/stdout is used \n"
					"\ttsgenuuid -v - ts engine version\n"
					"\ttsgenuuid -h - this help\n");

}

int parse_get_range(const char* range) {
	char* ptr;

	start = strtol(range, &ptr, 10);
	if(*ptr != ':' || start == -1)
		return 1;

	++ptr;
	if(*ptr == '$') {
		end = -1;
	}
	else {
		end = strtol(ptr, NULL, 10);
		if(end == -1)
			return 1;
	}

	return 0;
}

void parse_options_args(int argc, const char* argv[]) {
	int ok = 1;
	int c;
	int argi;

	boolean_t s_flag = B_FALSE;

	while((c = plat_getopt(argc, argv, "hvs:F:acg:")) != -1) {
		switch(c) {
		case 'v':
			print_ts_version("TS File utility");
			exit(0);
			break;
		case 's':
			strncpy(schema_path, optarg, PATHMAXLEN);
			s_flag = B_TRUE;
			break;
		case 'F':
			if(strcmp(optarg, "csv") == 0) {
				fputs("CSV is not supported. Sorry.\n", stderr);
				ok = 0;
			}
			else if(strcmp(optarg, "json") == 0) {
				backend = &json_backend;
			}
			else if(strcmp(optarg, "jsonraw") == 0) {
				backend = &jsonraw_backend;
			}
			else {
				ok = 0;
			}
			break;
		case 'a':
			command = COMMAND_ADD;
			break;
		case 'c':
			command = COMMAND_GET_COUNT;
			break;
		case 'g':
			command = COMMAND_GET_ENTRIES;
			if(parse_get_range(optarg) != 0) {
				fprintf(stderr, "Failed to parse get range '%s'\n", optarg);
				ok = 0;
			}
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

	if(!s_flag) {
		fputs("Missing schema file\n", stderr);
		usage();
		exit(1);
	}

	if(!ok) {
		usage();
		exit(1);
	}

	argi = optind;
	if(argi < argc) {
		strncpy(tsf_path, argv[argi], PATHMAXLEN);
	}
	else {
		fputs("Missing tsf-file\n", stderr);
		usage();
		exit(1);
	}

	++argi;
	if(argi < argc) {
		strncpy(file_path, argv[argi], PATHMAXLEN);
	}
	else {
		use_std_streams = B_TRUE;
	}
}

FILE* tsfutil_open_file(void) {
	if(command == COMMAND_GET_ENTRIES) {
		if(use_std_streams)
			return stdout;

		return fopen(file_path, "w");
	}

	if(use_std_streams)
		return stdin;

	return fopen(file_path, "r");
}

void tsfutil_close_file(FILE* file) {
	if(!use_std_streams)
		fclose(file);
}

int tsfutil_init(void) {
	int count = sizeof(subsys) / sizeof(struct subsystem);
	int i = 0;

	struct subsystem** subsys_list = (struct subsystem**)
			malloc(count * sizeof(struct subsystem*));

	for(i = 0; i < count; ++i ) {
		subsys_list[i] = &subsys[i];
	}

	atexit(ts_finish);
	return ts_init(subsys_list, count);
}

void tsfutil_error_msg(ts_errcode_t errcode, const char* format, ...) {
	va_list args;
	char error[256];

	va_start(args, format);
	vsnprintf(error, 256, format, args);
	va_end(args);

	logmsg(LOG_CRIT, error);
}

int do_command(void) {
	tsfile_t* ts_file;
	tsfile_schema_t* schema = schema_read(schema_path);
	FILE* file;

	int ret = 0;

	if(schema == NULL)
		return 1;

	if(command == COMMAND_CREATE) {
		ts_file = tsfile_create(tsf_path, schema);
	}
	else {
		ts_file = tsfile_open(tsf_path, schema);
	}

	mp_free(schema);
	if(ts_file == NULL) {
		return 1;
	}

	if(command != COMMAND_CREATE) {
		if(command == COMMAND_GET_COUNT) {
			uint32_t count = tsfile_get_count(ts_file);
			fprintf(stdout, "%lu\n", (unsigned long) count);
		}
		else {
			file = tsfutil_open_file();

			if(!file) {
				ret = 1;
				goto end;
			}

			if(command == COMMAND_GET_ENTRIES) {
				if(end == -1) {
					end = (int) tsfile_get_count(ts_file);
				}

				backend->get(file, ts_file, start, end);
			}
			else if(command == COMMAND_ADD) {
				backend->add(file, ts_file);
			}

			tsfutil_close_file(file);
		}
	}

end:
	tsfile_close(ts_file);
	return ret;
}

int main(int argc, char* argv[]) {
	int err = 0;
	int i;

	if(getenv("TS_DEBUG") != NULL) {
		log_debug = 1;
	}
	if(getenv("TS_TRACE") != NULL) {
		log_debug = 1;
		log_trace = 1;
	}

	parse_options_args(argc, argv);
	strncpy(log_filename, "-", LOGFNMAXLEN);

	tsfile_error_msg = tsfutil_error_msg;

	tsfutil_init();

	return do_command();
}
