/*
 * http.c
 *
 *  Created on: Dec 23, 2013
 *      Author: myaut
 */

#define LOG_SOURCE "http"
#include <log.h>

#include <mempool.h>
#include <defs.h>
#include <workload.h>
#include <wltype.h>
#include <modules.h>
#include <modapi.h>
#include <threads.h>
#include <netsock.h>

#include <http.h>

#include <curl/curl.h>

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
		wl_notify(wl, WLS_CFG_FAIL, -1, "Failed to open 'null' file", hwp->server);
		mp_free(hd);
		return 1;
	}

	wl->wl_private = hd;

	return 0;
}

MODEXPORT int http_wl_unconfig(workload_t* wl) {
	mp_free(wl->wl_private);

	return 0;
}

MODEXPORT int http_run_request(request_t* rq) {
  CURL *curl;
  CURLcode res;
  long status;

  struct http_workload* hwp = (struct http_workload*) rq->rq_workload->wl_params;
  struct http_request* hrq = (struct http_request*) rq->rq_params;
  struct http_data* hd = (struct http_data*) rq->rq_workload->wl_private;

  const char url[MAXURILEN];

  snprintf(url, MAXURILEN, "http://%s%s", hd->serveraddr, hrq->uri);

  curl = curl_easy_init();
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

	curl_easy_cleanup(curl);

	return 0;
  }

  return 1;
}

wl_type_t http_wlt = {
	"http",							/* wlt_name */

	WLC_NET_CLIENT,					/* wlt_class */

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
