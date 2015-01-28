
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
*/    



#include <tsload/defs.h>

#include <tsload/hashmap.h>
#include <tsload/getopt.h>

#include <tsload.h>
#include <tsload/load/wltype.h>
#include <tsload/load/wlparam.h>

#include <commands.h>
#include <info.h>

int tse_print_wltype_walker(hm_item_t* item, void* context);
void tse_print_range(wlp_descr_t* wlp, char* buf, size_t buflen);
void tse_print_default(wlp_descr_t* wlp, char* buf, size_t buflen);

const char* ts_wlt_type_names[WLP_TYPE_MAX] = {
	"NULL",
	"BOOL",
	"INT",
	"FLOAT",
	"STR",
	"STRSET",

	"SIZE",
	"TIME",
	"PATH",
	"CPUOBJ",
	"DISK"
};

int tse_list_wltypes(experiment_t* root, int argc, char* argv[]) {
	boolean_t json = B_FALSE;
	json_node_t* node = NULL;
	json_node_t* item = NULL;

	int c;
	int argi;

	void* ptr;
	
	int flags = INFO_XDATA | INFO_HEADER;

	fputs("WARNING: `wl` and `workload` subcommands are deprecated. "
		  "Use `info wl` or `info workload` instead\n", stderr);

	while((c = plat_getopt(argc, argv, "j")) != -1) {
		switch(c) {
		case 'j':
			json = B_TRUE;
			break;
		case '?':
			tse_command_error_msg(CMD_INVALID_OPT, "Invalid suboption -%c for command wl\n", optopt);
			return CMD_INVALID_OPT;
		}
	}

	/* If workload types are specified in argv, first check that they are exist  */
	argi = optind;
	for( ; argi < argc; ++argi) {
		ptr = tsload_walk_workload_types(TSLOAD_WALK_FIND, argv[argi], NULL);
		if(ptr == NULL) {
			tse_command_error_msg(CMD_NOT_EXISTS, "Invalid workload type '%s' - "
								  "not registered within TSLoad", argv[argi]);
			return CMD_NOT_EXISTS;
		}
	}

	if(!json) {
		printf("%-16s %-18s %-12s\n", "WLTYPE", "CLASS", "MODULE");
	}

	argi = optind;
	if(argi == argc) {
		if(json) {
			node = tsload_walk_workload_types(TSLOAD_WALK_TSOBJ_ALL, NULL, NULL);
		}
		else {
			tsload_walk_workload_types(TSLOAD_WALK_WALK, NULL, tse_print_wltype_walker);
		}
	}
	else {
		if(json)
			node = json_new_node(NULL);

		for( ; argi < argc; ++argi) {
			if(json) {
				item = tsload_walk_workload_types(TSLOAD_WALK_TSOBJ, argv[argi], NULL);
				if(item) {
					json_add_node(node, json_str_create(argv[argi]), item);
				}
			}
			else {
				ptr = tsload_walk_workload_types(TSLOAD_WALK_FIND, argv[argi], NULL);
				tse_print_wltype((wl_type_t*) ptr, &flags);
			}
		}
	}

	if(json) {
		json_write_file(node, stdout, B_TRUE);
		json_node_destroy(node);
	}

	return CMD_OK;
}

struct tse_wlt_class {
	int mask;
	int i1; char c1;
	int i2; char c2;
};

#define TSE_WLT_CLASSES_COUNT			13

struct tse_wlt_class tse_wlt_class_chars[] = {
	{WLC_CPU_INTEGER, 			0, 'C', 	1, 'i'},
	{WLC_CPU_FLOAT, 			0, 'C', 	2, 'f'},
	{WLC_CPU_MEMORY, 			0, 'C', 	3, 'm'},
	{WLC_CPU_MISC, 				0, 'C', 	-1, '\0'},
	{WLC_MEMORY_ALLOCATION, 	4, 'M', 	5, 'a'},
	{WLC_FILESYSTEM_OP, 		6, 'F', 	7, 'o'},
	{WLC_FILESYSTEM_RW, 		6, 'F', 	8, 'r'},
	{WLC_FILESYSTEM_RW, 		6, 'F', 	9, 'w'},
	{WLC_DISK_RW, 				10, 'D', 	11, 'r'},
	{WLC_DISK_RW, 				10, 'D', 	12, 'w'},
	{WLC_NETWORK, 				13, 'N', 	14, 'n'},
	{WLC_NET_CLIENT, 			14, 'N', 	15, 'c'},
	{WLC_OS_BENCHMARK, 			16, 'O', 	-1, '\0'},
};

void tse_print_wltype(void* obj, void* p_flags) {
	wl_type_t* wlt = (wl_type_t*) obj;
	int flags = * (int*) p_flags;
	
	char wlt_class[18];
	int i;
	struct tse_wlt_class* clch;

	wlp_descr_t* wlp;
	const char* scope;
	const char* type;
	const char* opt;

	char range[64];
	char defval[128];

	wlp = &wlt->wlt_params[0];
	strcpy(wlt_class, "------------------");

	/* Generate text for bitmap of workload class */
	for(i = 0; i < TSE_WLT_CLASSES_COUNT; ++i) {
		clch = &tse_wlt_class_chars[i];
		if(wlt->wlt_class & clch->mask) {
			wlt_class[clch->i1] = clch->c1;
			if(clch->i2 >= 0)
				wlt_class[clch->i2] = clch->c2;
		}
	}

	printf("%-16s %-18s %-12s\n", wlt->wlt_name, wlt_class,
			wlt->wlt_module->mod_name);

	if(flags & INFO_XDATA) {
		tse_print_description(wlt->wlt_description);
		
		if(wlp->type != WLP_NULL && (flags & INFO_HEADER)) {
			printf("\t%-16s %-4s %-4s %-6s %-10s %-6s\n",
					"PARAM", "SCOPE", "OPT", "TYPE", "RANGE", "DEFAULT");
		}

		/* Print params */
		while(wlp->type != WLP_NULL) {
			scope = ((wlp->flags & WLPF_OUTPUT) == WLPF_OUTPUT)
						? "OUT"
						: (wlp->flags & WLPF_REQUEST)
							? "RQ"
							: "WL";
			opt = (wlp->flags & WLPF_OPTIONAL) ? "OPT" : "";
			type = ts_wlt_type_names[wlp->type];

			tse_print_range(wlp, range, 64);
			tse_print_default(wlp, defval, 128);

			printf("\t%-16s %-4s %-4s %-6s %-10s %-6s\n", wlp->name,
						scope, opt, type, range, defval);
			printf("\t\t%s\n", wlp->description);

			++wlp;
		}
	}
}

void tse_print_range(wlp_descr_t* wlp, char* buf, size_t buflen) {
	*buf = '\0';

	if(wlp->range.range) {
		switch(wlp_get_base_type(wlp)) {
		case WLP_INTEGER:
			snprintf(buf, buflen, "[%lld..%lld]",
						(unsigned long long) wlp->range.i_min,
						(unsigned long long) wlp->range.i_max);
			break;
		case WLP_FLOAT:
			snprintf(buf, buflen, "[%f..%f]", wlp->range.d_min, wlp->range.d_max);
			break;
		case WLP_RAW_STRING:
			snprintf(buf, buflen, "len: %u",
						wlp->range.str_length);
			break;
		case WLP_STRING_SET:
			{
				int i;
				size_t len = 0;
				const char* fmtstr = "%s";

				for(i = 0; i < wlp->range.ss_num; ++i) {
					len += snprintf(buf + len, buflen - len, fmtstr, wlp->range.ss_strings[i]);

					if(len >= buflen)
						break;

					fmtstr = ",%s";
				}
			}
			break;
		}
	}
}

void tse_print_default(wlp_descr_t* wlp, char* buf, size_t buflen) {
	*buf = '\0';

	if(wlp->defval.enabled) {
		switch(wlp_get_base_type(wlp)) {
		case WLP_BOOL:
			strncpy(buf, wlp->defval.b ? "true" : "false", buflen);
			break;
		case WLP_INTEGER:
			snprintf(buf, buflen, "%lld",
						(unsigned long long) wlp->defval.i);
			break;
		case WLP_FLOAT:
			snprintf(buf, buflen, "%f", wlp->defval.f);
			break;
		case WLP_RAW_STRING:
			strncpy(buf, wlp->defval.s, buflen);
			break;
		case WLP_STRING_SET:
			strncpy(buf, wlp->range.ss_strings[wlp->defval.ssi], buflen);
			break;
		}
	}
}

int tse_print_wltype_walker(hm_item_t* item, void* context) {
	wl_type_t* wlt = (wl_type_t*) item;
	int flags = INFO_XDATA | INFO_HEADER;
	tse_print_wltype(wlt, &flags);

	return HM_WALKER_CONTINUE;
}

