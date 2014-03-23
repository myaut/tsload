/*
 * http.h
 *
 *  Created on: Dec 23, 2013
 *      Author: myaut
 */

#ifndef MOD_LOAD_HTTP_
#define MOD_LOAD_HTTP_

#include <netsock.h>
#include <wlparam.h>

#include <stdio.h>

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
	FILE*			fnull;
};

struct http_request {
	wlp_string_t 	uri[MAXURILEN];
	wlp_integer_t	status;
};

PLATAPI FILE* plat_open_null(void);

#endif
