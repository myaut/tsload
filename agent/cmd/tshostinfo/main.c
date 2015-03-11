/*
 * main.c
 *
 * Main file for ts-loader daemon
 */


#include <tsload/defs.h>

#include <tsload/init.h>
#include <tsload/getopt.h>
#include <tsload/version.h>
#include <tsload/tuneit.h>
#include <tsload/execpath.h>
#include <tsload/pathutil.h>
#include <tsload/geninstall.h>

#include <hostinfo/hiobject.h>

#include <tsload/json/json.h>

#include <hiprint.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

boolean_t hi_mod_configured = B_FALSE;

int print_flags = INFO_DEFAULT;

int init(void);
void usage(int ret, const char* reason, ...);

/*
 * Make TSLoad a bit portable - get absolute path of run-tsload,
 * delete INSTALL_BIN from it and append INSTALL_VAR or INSTALL_MOD_PATH
 * to find path to log and modules.
 * */
void deduce_paths(void) {
	const char* cur_execpath = plat_execpath();
	path_split_iter_t iter;
	char root[PATHMAXLEN];

	const char* cur_dirpath = path_dirname(&iter, cur_execpath);

	if(path_remove(root, PATHMAXLEN, cur_dirpath, INSTALL_BIN) != NULL) {
		path_join(hi_obj_modpath, PATHMAXLEN, root, INSTALL_LIB, NULL);
		
		hi_mod_configured = B_TRUE;
	}
}

void parse_options(int argc, char* argv[]) {
	int c;

	while((c = plat_getopt(argc, argv, "vhlxjX:")) != -1) {
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
		case 'j':
			print_flags |= INFO_JSON;
			break;
		case 'X':
			tuneit_add_option(optarg);
			break;
		case 'h':
			usage(0, "");
			break;
		case '?':
			usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
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
	PRINT_INFO_IMPL(fs);
	PRINT_INFO_IMPL(cpu);
	PRINT_INFO_IMPL(sched);
	PRINT_INFO_IMPL(vm);
	PRINT_INFO_IMPL(net);

	return 1;
}

void read_environ() {
	char* env_hi_mod_path = getenv("TS_HIMODPATH");
	
	if(env_hi_mod_path) {
		strncpy(hi_obj_modpath, env_hi_mod_path, PATHMAXLEN);
		hi_mod_configured = B_TRUE;
	}
}

int print_json(void) {
	json_node_t* hiobj = (json_node_t*) tsobj_hi_format_all(B_FALSE);

	return json_write_file(hiobj, stdout, B_TRUE);
}

int main(int argc, char* argv[]) {
	int err = 0;
	int i;
	
	deduce_paths();
	read_environ();
	parse_options(argc, argv);
	
	if(!hi_mod_configured) {
		usage(1, "Missing TS_HIMODPATH environment variable and failed to deduce HostInfo modpath\n");
	}
	
	init();

	if(print_flags & INFO_JSON) {
		return print_json();
	}

	if((argc - optind) == 0) {
		print_flags |= INFO_ALL;
		print_info("");
		exit(0);
	}

	for(i = optind; i < argc; ++i) {
		if(print_info(argv[i]) != 0) {
			usage(1, "Invalid topic %s\n", argv[i]);
		}
	}

	return 0;
}
