/*
 * modapi.h
 *
 *  Created on: 26.11.2012
 *      Author: myaut
 */

#ifndef MODAPI_H_
#define MODAPI_H_

#include <defs.h>

#define MODNAMELEN			    32

#define MOD_API_VERSION		0x0002

#define MOD_TSLOAD		1
#define MOD_MONITOR		2

#define MODEXPORT		LIBEXPORT

#define DECLARE_MODAPI_VERSION(version) \
		LIBEXPORT int mod_api_version = version
#define DECLARE_MOD_TYPE(type) \
		LIBEXPORT int mod_type = type
#define DECLARE_MOD_NAME(name)	\
		LIBEXPORT char mod_name[MODNAMELEN] = name

struct module;
typedef int (* mod_config_func)(struct module* mod);

#define	MOD_OK			0
#define MOD_ERROR		-1

#endif /* MODAPI_H_ */
