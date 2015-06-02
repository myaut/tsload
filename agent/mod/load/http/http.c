
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



#define LOG_SOURCE "http"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/modapi.h>
#include <tsload/netsock.h>

#include <tsload/load/workload.h>
#include <tsload/load/wltype.h>

#include <http.h>

#include <stdlib.h>
#include <string.h>


DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("http");
DECLARE_MOD_TYPE(MOD_TSLOAD);

MODEXPORT wlp_descr_t http_params[] = {
	{ WLP_RAW_STRING, WLPF_NO_FLAGS,
		WLP_STRING_LENGTH(MAXHOSTNAMELEN),
		WLP_STRING_DEFAULT("localhost"),
		"server",
		"HTTP server hostname",
		offsetof(struct http_workload, server)
	},
	{ WLP_INTEGER, WLPF_NO_FLAGS,
		WLP_NO_RANGE(),
		WLP_INT_DEFAULT(HTTP_DEFAULT_PORT),
		"port",
		"HTTP server port",
		offsetof(struct http_workload, port) },
	{ WLP_RAW_STRING, WLPF_REQUEST,
		WLP_STRING_LENGTH(MAXURILEN),
		WLP_NO_DEFAULT(),
		"uri",
		"HTTP request uri",
		offsetof(struct http_request, uri)
	},
	{ WLP_INTEGER, WLPF_OUTPUT,
		WLP_NO_RANGE(),
		WLP_NO_DEFAULT(),
		"status",
		"HTTP response status",
		offsetof(struct http_request, status)
	},
	{ WLP_NULL }
};

module_t* self = NULL;
thread_mutex_t resolver_mutex;

MODEXPORT int http_wl_config(workload_t* wl) {
	struct http_workload* hwp = (struct http_workload*) wl->wl_params;
	struct http_data* hd = mp_malloc(sizeof(struct http_data));

	nsk_host_entry he;
	nsk_addr addr;

	unsigned num_workers = wl->wl_tp->tp_num_threads;
	int wid;

	/* Preliminary resolve hostname to reduce pressure on DNS server
	 * (in some cases HTTP benchmark became DNS benchmark)
	 *
	 * FIXME: If server is load-balanced via DNS, only first entry would be used (see nsk code) */
	if(nsk_resolve(hwp->server, &he) != NSK_OK) {
		wl_notify(wl, WLS_CFG_FAIL, -1, "Couldn't resolve host '%s'", hwp->server);
		mp_free(hd);
		return 1;
	}

	nsk_setaddr(&addr, &he, hwp->port);

	if(nsk_addr_to_string(&addr, hd->serveraddr, MAXHOSTNAMELEN,
			HTTP_DEFAULT_PORT, NSK_ADDR_FLAG_DEFAULT) != NSK_OK) {
		wl_notify(wl, WLS_CFG_FAIL, -1, "Couldn't create address string for '%s'", hwp->server);
		mp_free(hd);
		return 1;
	}

	hd->fnull = plat_open_null();
	if(hd->fnull == NULL) {
		wl_notify(wl, WLS_CFG_FAIL, -1, "Failed to open 'null' file");
		mp_free(hd);
		return 1;
	}

	hd->curl = mp_malloc(sizeof(CURL*) * num_workers);
	hd->num_workers = num_workers;
	for(wid = 0; wid < num_workers; ++wid) {
		hd->curl[wid] = curl_easy_init();
	}

	wl->wl_private = hd;

	return 0;
}

MODEXPORT int http_wl_unconfig(workload_t* wl) {
	struct http_data* hd = (struct http_data*) wl->wl_private;

	unsigned num_workers = hd->num_workers;
	int wid;

	for(wid = 0; wid < num_workers; ++wid) {
		curl_easy_cleanup(hd->curl[wid]);
	}

	mp_free(wl->wl_private);

	return 0;
}

MODEXPORT int http_run_request(request_t* rq) {
  CURL* curl;
  CURLcode res;
  long status;

  struct http_workload* hwp = (struct http_workload*) rq->rq_workload->wl_params;
  struct http_request* hrq = (struct http_request*) rq->rq_params;
  struct http_data* hd = (struct http_data*) rq->rq_workload->wl_private;

  char url[MAXURILEN];

  snprintf(url, MAXURILEN, "http://%s%s", hd->serveraddr, hrq->uri);

  curl = hd->curl[rq->rq_thread_id];

  if(curl) {
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	/* FIXME: Should be parameter */
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
	curl_easy_setopt(curl, CURLOPT_FILE, hd->fnull);

	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
	  logmsg(LOG_WARN, "curl_easy_perform() failed: %s\n",
			  	  	    curl_easy_strerror(res));
	}
	else {
		/* TODO: More curl information, i.e. times */
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
		hrq->status = status;
	}

	return 0;
  }

  return 1;
}

wl_type_t http_wlt = {
	AAS_CONST_STR("http"),			/* wlt_name */

	WLC_NET_CLIENT,					/* wlt_class */
	
	"Uses cURL to generate HTTP requests. URI is a request parameter, so "
	"you may use probability maps, to variate your workload. ",

	http_params,					/* wlt_params */
	sizeof(struct http_workload),	/* wlt_params_size */
	sizeof(struct http_request),	/* wlt_rqparams_size  */

	http_wl_config,					/* wlt_wl_config */
	http_wl_unconfig,				/* wlt_wl_unconfig */

	NULL,							/* wlt_wl_step */

	http_run_request				/* wlt_run_request */
};

MODEXPORT int mod_config(module_t* mod) {
	self = mod;

	wl_type_register(mod, &http_wlt);

	return MOD_OK;
}

MODEXPORT int mod_unconfig(module_t* mod) {
	wl_type_unregister(mod, &http_wlt);

	return MOD_OK;
}

