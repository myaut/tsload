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

tsf_backend_t* backend = NULL;

int command = COMMAND_CREATE;

char tsf_path[PATHMAXLEN];
char schema_path[PATHMAXLEN];

int start = 0;
int end = -1;

extern boolean_t tsfutil_json_print_one;

char file_path[PATHMAXLEN];
boolean_t use_std_streams = B_FALSE;

LIBEXPORT struct subsystem subsys[] = {
	SUBSYSTEM("log", log_init, log_fini),
	SUBSYSTEM("mempool", mempool_init, mempool_fini),
	SUBSYSTEM("threads", threads_init, threads_fini),
	SUBSYSTEM("tsfile", tsfile_init, tsfile_fini)
};

void usage(int ret, const char* reason, ...);

int parse_get_range(const char* range) {
	char* ptr;

	start = strtol(range, &ptr, 10);

	if(*ptr == '\0') {
		/* Special case for JSON - get one entry */
		end = start + 1;
		return 0;
	}

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

	while((c = plat_getopt(argc, argv, "+hvs:F:")) != -1) {
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
			backend = tsfile_backend_create(optarg);

			if(backend == NULL) {
				usage(1, "Invalid backend '%s'\n", optarg);
			}
			break;
		case 'h':
			usage(0, "");
			break;
		case '?':
			usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}

	if(!s_flag) {
		usage(1, "Missing schema file\n");
	}

	argi = optind;
	if(argi == argc) {
		usage(1, "Missing subcommand\n");
	}

	if(strcmp(argv[argi], "get") == 0) {
		command = COMMAND_GET_ENTRIES;
	}
	else if(strcmp(argv[argi], "add") == 0) {
		command = COMMAND_ADD;
	}
	else if(strcmp(argv[argi], "count") == 0) {
		command = COMMAND_GET_COUNT;
	}
	else if(strcmp(argv[argi], "create") == 0) {
		command = COMMAND_CREATE;
	}
	else {
		usage(1, "Unknown subcommand '%s'\n", argv[argi]);
	}
	++optind;

	while((c = plat_getopt(argc, argv, "g:o:")) != -1) {
		switch(c) {
		case 'g':
			if(parse_get_range(optarg) != 0) {
				fprintf(stderr, "Failed to parse get range '%s'\n", optarg);
				ok = 0;
			}
			break;
		case 'o':
			ok = tsfile_backend_set(backend, optarg);
			if(!ok) {
				usage(1, "Unknown backend option '%s'\n", optarg);
			}
			break;
		case '?':
			usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}

	argi = optind;
	if(argi < argc) {
		strncpy(tsf_path, argv[argi], PATHMAXLEN);
	}
	else {
		usage(1, "Missing tsf-file\n");
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
	tsfile_schema_t* schema = tsfile_schema_read(schema_path);
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

			tsfile_backend_set_files(backend, file, ts_file);

			if(command == COMMAND_GET_ENTRIES) {
				if(end == -1) {
					end = (int) tsfile_get_count(ts_file);
				}

				ret = tsfile_backend_get(backend, start, end);
			}
			else if(command == COMMAND_ADD) {
				ret = tsfile_backend_add(backend);
			}

			tsfutil_close_file(file);
		}
	}

end:
	if(ret != 0) {
		fputs("Failure occured. See log for details\n", stderr);
	}

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
