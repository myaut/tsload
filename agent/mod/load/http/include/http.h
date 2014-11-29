
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef MOD_LOAD_HTTP_
#define MOD_LOAD_HTTP_

#include <tsload/defs.h>

#include <tsload/load/wlparam.h>

#include <stdio.h>

#include <curl/curl.h>


#define MAXHOSTNAMELEN	256
#define MAXURILEN		2048

#define HTTP_DEFAULT_PORT	80

#define RESPONSE_BUF_SIZE	4096
#define USER_AGENT			"User-Agent: TSLoad HTTP Module"

struct http_workload {
	wlp_string_t 	server[MAXHOSTNAMELEN];
	wlp_integer_t 	port;
};

struct http_data {
	wlp_string_t 	serveraddr[MAXHOSTNAMELEN];
	FILE* fnull;

	CURL** curl;
	unsigned num_workers;
};

struct http_request {
	wlp_string_t 	uri[MAXURILEN];
	wlp_integer_t	status;
};

PLATAPI FILE* plat_open_null(void);

#endif

