
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

#include <tsload/load/randgen.h>
#include <tsload/load/tpdisp.h>
#include <tsload/load/rqsched.h>
#include <tsload.h>

#include <commands.h>
#include <info.h>

#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>

/* For description printing */
int tse_print_maxline = 80;

static struct tse_info_printer tse_info_printers[] = {
	{
		"wl", "workload",
		{ { 16 , "WLTYPE"}, { 18, "CLASS" }, { 0, "MODULE" }, 
		  { 0, NULL } },
		tsload_walk_workload_types,
		tse_print_wltype
	},
	{
		"rg", "randgen",
		{ { 16 , "RANDGEN"}, { 6, "SGL?" }, { 21, "MAXVAL" }, 
		  { 0, "MODULE" }, { 0, NULL } },
		tsload_walk_random_generators,
		tse_print_randgen
	},
	{
		"rv", "randvar",
		{ { 16 , "RANDVAR"}, { 0, "MODULE" }, { 0, NULL } },
		tsload_walk_random_variators,
		tse_print_randvar
	},
	{
		"tpd", "tpdisp",
		{ { 16 , "TPDISP"}, { 0, "MODULE" }, { 0, NULL } },
		tsload_walk_tp_dispatchers,
		tse_print_tpdisp
	},
	{
		"rqsvar", NULL,
		{ { 16 , "RQSVAR"}, { 16 , "VARIATOR"}, { 0, "MODULE" }, { 0, NULL } },
		tsload_walk_rqsched_variators,
		tse_print_rqsvar
	},
	{
		"rqs", "rqsched",
		{ { 16 , "RQSCHED"}, { 5, "VAR?"}, { 0, "MODULE" }, { 0, NULL } },
		tsload_walk_request_schedulers,
		tse_print_rqsched
	},
	{
		NULL, NULL, { {0, NULL} }, NULL, NULL
	}
};

int tse_info(experiment_t* root, int argc, char* argv[]) {
	boolean_t json = B_FALSE;
	json_node_t* node = NULL;
	json_node_t* item = NULL;

	int c;
	int ret;
	int argi;

	int flags = INFO_HEADER;
	
	void* ptr;
	
	char* topic;
	
	struct tse_info_printer* printer = tse_info_printers;
	
	/* Find appropriate printer */
	if(optind == argc) {
		tse_command_error_msg(CMD_INVALID_ARG, "Please specify type of object to get info\n");
		return CMD_INVALID_ARG;
	}
	
	topic = argv[optind++];
	while(printer->topic != NULL) {
		if(strcmp(topic, printer->topic) == 0)
			break;
		if(printer->alias && strcmp(topic, printer->alias) == 0)
			break;
		++printer;
	}	
	if(printer->topic == NULL) {
		tse_command_error_msg(CMD_INVALID_ARG, "Invalid `info` subcommand '%s' specified\n", topic);
		return CMD_INVALID_ARG;	
	}
	
	/* Parse arguments */
	while((c = plat_getopt(argc, argv, "jHlx")) != -1) {
		switch(c) {
		case 'j':
			json = B_TRUE;
			break;
		case 'H':
			flags &= ~INFO_HEADER;
			break;
		case 'l':
			flags |= INFO_LEGEND;
			break;
		case 'x':
			flags |= INFO_XDATA;
			break;
		case '?':
			tse_command_error_msg(CMD_INVALID_OPT, "Invalid suboption -%c for command `info`\n", optopt);
			return CMD_INVALID_OPT;
		}
	}

	/* If objects are specified in argv, first check that they are exist  */
	argi = optind;
	for( ; argi < argc; ++argi) {
		ptr = printer->walker(TSLOAD_WALK_FIND, argv[argi], NULL);
		if(ptr == NULL) {
			tse_command_error_msg(CMD_NOT_EXISTS, "Invalid object '%s' - "
								  "not registered within TSLoad", argv[argi]);
			return CMD_NOT_EXISTS;
		}
	}
	
	if(!json && flags & INFO_HEADER) {
		tse_print_header(printer->header);
	}

	argi = optind;
	if(argi == argc) {
		if(json) {
			node = printer->walker(TSLOAD_WALK_TSOBJ_ALL, NULL, NULL);
		}
		else {
			printer->flags = flags;
			printer->walker(TSLOAD_WALK_WALK, printer, tse_print_walker);
		}
	}
	else {
		if(json)
			node = json_new_node(NULL);

		for( ; argi < argc; ++argi) {
			if(json) {
				item = printer->walker(TSLOAD_WALK_TSOBJ, argv[argi], NULL);
				if(item) {
					json_add_node(node, json_str_create(argv[argi]), item);
				}
			}
			else {
				ptr = printer->walker(TSLOAD_WALK_FIND, argv[argi], NULL);
				printer->printer(ptr, &flags);
			}
		}
	}

	if(json) {
		json_write_file(node, stdout, B_TRUE);
		json_node_destroy(node);
	}

	return CMD_OK;
}

static const char* tse_info_modname(module_t* mod) {
	if(mod == NULL)
		return "TSLoad Core";
	return mod->mod_name;
}

#define TSE_PRINT_RANDGEN_MAX(limit)			\
	case limit:									\
		printf("%-21s", # limit);				\
		break;

void tse_print_randgen(void* obj, void* p_flags) {
	randgen_class_t* rg_class = (randgen_class_t*) obj;
	int flags = * (int*) p_flags;
	
	printf("%-16s %-6s ", rg_class->rg_class_name, 
		   (rg_class->rg_is_singleton) ? "SGL" : "");
	
	switch(rg_class->rg_max) {
		TSE_PRINT_RANDGEN_MAX(ULLONG_MAX)
		TSE_PRINT_RANDGEN_MAX(LLONG_MAX)
#if (ULLONG_MAX > ULONG_MAX)
		TSE_PRINT_RANDGEN_MAX(ULONG_MAX)
		TSE_PRINT_RANDGEN_MAX(LONG_MAX)
#endif
#if (ULONG_MAX > UINT_MAX)
		TSE_PRINT_RANDGEN_MAX(UINT_MAX)
		TSE_PRINT_RANDGEN_MAX(INT_MAX)
#endif		
		default:
			printf("%-21" PRId64, rg_class->rg_max);
			break;
	}
	
	printf(" %s\n", tse_info_modname(rg_class->rg_module));
}

void tse_print_randvar(void* obj, void* p_flags) {
	randvar_class_t* rv_class = (randvar_class_t*) obj;
	int flags = * (int*) p_flags;
	
	printf("%-16s %s\n", rv_class->rv_class_name, tse_info_modname(rv_class->rv_module));
	
	if(flags & INFO_XDATA) {
		tse_print_params(flags, rv_class->rv_params);
	}
}

void tse_print_tpdisp(void* obj, void* p_flags) {
	tp_disp_class_t* tpd_class = (tp_disp_class_t*) obj;
	int flags = * (int*) p_flags;
	
	printf("%-16s %s\n", tpd_class->name, 
		   tse_info_modname(tpd_class->mod));
	
	if(flags & INFO_XDATA) {
		tse_print_description(tpd_class->description);	
		tse_print_params(flags, tpd_class->params);
	}
}

void tse_print_rqsvar(void* obj, void* p_flags) {
	rqsvar_class_t* var_class = (rqsvar_class_t*) obj;
	int flags = * (int*) p_flags;
	
	printf("%-16s %-16s %s\n", var_class->rqsvar_name, 
		   var_class->rqsvar_rvclass->rv_class_name,
		   tse_info_modname(var_class->rqsvar_module));
	
	if(flags & INFO_XDATA) {
		tse_print_params(flags, var_class->rqsvar_params);
	}
}

void tse_print_rqsched(void* obj, void* p_flags) {
	rqsched_class_t* rqs_class = (rqsched_class_t*) obj;
	int flags = * (int*) p_flags;
	
	printf("%-16s %-5s %s\n", rqs_class->rqsched_name,
		   (rqs_class->rqsched_flags & RQSCHED_NEED_VARIATOR)? "+" : "",
		   tse_info_modname(rqs_class->rqsched_module));
	
	if(flags & INFO_XDATA) {
		tse_print_description(rqs_class->rqsched_description);
		tse_print_params(flags, rqs_class->rqsched_params);
	}
}

void tse_print_header(struct tse_info_printer_header* header) {
	char fmtstr[8];
	
	while(header->colhdr != NULL) {	
		if(header->colwidth == 0) {
			printf("%s\n", header->colhdr);
			return;
		}
		
		snprintf(fmtstr, 8, "%%-%ds ", header->colwidth);
		printf(fmtstr, header->colhdr);
		
		++header;
	}
}

static const char* tse_param_types[][3] = {
	{ "", 		"ARRAY[]", 			"MAP[]" },
	{ "BOOL", 	"ARRAY[BOOL]",		"MAP[BOOL]" },
	{ "INT", 	"ARRAY[INT]", 		"MAP[INT]" },
	{ "FLOAT", 	"ARRAY[FLOAT]",		"MAP[FLOAT]" },
	{ "STR", 	"ARRAY[STR]", 		"MAP[STR]" },
	{ "RANDGEN", "ARRAY[RANDGEN]", "MAP[RANDGEN]" },
	{ "RANDVAR", "ARRAY[RANDVAR]", "MAP[RANDVAR]" }
};		

const char* tse_param_type_str(int type) {
	int basetype = type & TSLOAD_PARAM_MASK;
	
	if(basetype > TSLOAD_PARAM_RANDVAR)
		return "???";
	
	if(type & TSLOAD_PARAM_ARRAY_FLAG)
		return tse_param_types[basetype][1];
	else if(type & TSLOAD_PARAM_MAP_FLAG)
		return tse_param_types[basetype][1];
	
	return tse_param_types[basetype][0];
}

void tse_print_params(int flags, tsload_param_t* params) {
	tsload_param_t* param = params;
	
	if((flags & INFO_HEADER) && (param->type != TSLOAD_PARAM_NULL))
		printf("\t%-10s %-14s %s\n", "PARAM", "TYPE", "HINT");
	
	while(param->type != TSLOAD_PARAM_NULL) {
		printf("\t%-10s %-14s %s\n", param->name, 
			   tse_param_type_str(param->type), param->hint );
		++param;
	}
}

void tse_print_description(const char* description) {
	/* Save 8 bytes for tabstop */
	int maxline = tse_print_maxline - 8;
	const char* p = description;
	const char* end = description;
	
	AUTOSTRING char* line;	
		
	while(strlen(p) > maxline) {
		end += maxline;
		
		/* Find previous space character  */
		while(!isspace(*end)) {
			if(end <= p) {
				/* Failed to find space in this chunk of space -- simply split */
				end = p + maxline;
				break;
			}			
			--end;
		}
		
		aas_copy_n(aas_init(&line), p, end - p);
		printf("\t%s\n", line);
		aas_free(&line);
		
		p = ++end;
	}
	
	printf("\t%s\n", p);	
}

int tse_print_walker(hm_item_t* item, void* context) {
	struct tse_info_printer* printer = (struct tse_info_printer*) context;
	
	printer->printer(item, &printer->flags);

	return HM_WALKER_CONTINUE;
}