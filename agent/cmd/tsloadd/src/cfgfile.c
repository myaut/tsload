
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#include <log.h>
#include <modules.h>
#include <client.h>
#include <agent.h>

#include <defs.h>

#include <cfgfile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * config.c
 *
 * @brief Reads tsload parameters from configuration file
 *
 * File format: NAME=value - configuration variables
 * Empty lines or comments (starting with #) are ignored
 *
 * Variables:
 * MODPATH - path to modules
 * LOGFILE - log file destination (may be - for stdout)
 * CLIENTHOST, CLIENTPORT - Path to TSLoad server
 * AGENTUUID - agent uuid used to register on TSLoad server
 *
 * NOTE: because config file is read before logging is configured
 * it writes all error messages directly to stderr
 */

LIBIMPORT char log_filename[LOGFNMAXLEN];
LIBIMPORT char mod_search_path[MODPATHLEN];
LIBIMPORT int clnt_port;
LIBIMPORT char clnt_host[CLNTHOSTLEN];
/* LIBIMPORT char agent_uuid[AGENTUUIDLEN]; */

/**
 * Process single config file's option:
 *
 * @return CFG_OK or aq if option is unknown
 * */
int cfg_option(char* name, char* value) {
	if(strcmp(name, "MODPATH") == 0) {
		strncpy(mod_search_path, value, MODPATHLEN);
	}
	else if(strcmp(name, "LOGFILE") == 0) {
		strncpy(log_filename, value, LOGFNMAXLEN);
	}
	else if(strcmp(name, "CLIENTHOST") == 0) {
		strncpy(clnt_host, value, CLNTHOSTLEN);
	}
	else if(strcmp(name, "CLIENTPORT") == 0) {
		clnt_port = atoi(value);
	}
	else if(strcmp(name, "AGENTUUID") == 0) {
		strncpy(agent_uuid, value, AGENTUUIDLEN);
	}
	else {
		return CFG_ERR_INVALID_OPTION;
	}

	return CFG_OK;
}

/**
 * Read and parse config file
 */
int cfg_read(FILE* cfg_file) {
	char line[CONFLINELEN];
	char* eq_pos = NULL;		/*Position of '=' sign*/
	size_t len;
	int line_num = 1;

	while(fgets(line, CONFLINELEN, cfg_file) != NULL) {
		len = strlen(line);

		/*Remove whitespaces from end of the line*/
		while(isspace(line[--len]) && len != 0);
		line[len + 1] = 0;

		/* If line is comment or just empty */
		if(line[0] == '#' || len == 0)
			continue;

		eq_pos = strchr(line, '=');

		/*Line does not contain =*/
		if(eq_pos == NULL) {
			fprintf(stderr, "Not found '=' on line %d\n", line_num);
			return CFG_ERR_MISSING_EQ;
		}

		/*Split line into two substrings*/
		*eq_pos = '\0';

		if(cfg_option(line, eq_pos + 1) != 0) {
			fprintf(stderr, "Invalid option %s on line %d\n", line, line_num);
			return CFG_ERR_INVALID_OPTION;
		}

		line_num++;
	}

	return CFG_OK;
}

int cfg_init(const char* cfg_file_name) {
	int ret = CFG_OK;

	FILE* cfg_file = fopen(cfg_file_name, "r");

	if(!cfg_file) {
		fprintf(stderr, "Failed to open file '%s'\n", cfg_file_name);
		return CFG_ERR_NOFILE;
	}

	ret = cfg_read(cfg_file);

	fclose(cfg_file);

	return ret;
}

