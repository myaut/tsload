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

/**
 * Module types
 */
#define MOD_TSLOAD		1
#define MOD_MONITOR		2

#define MODEXPORT		LIBEXPORT

/**
 * Module head declarations
 */
#define DECLARE_MODAPI_VERSION(version) \
		MODEXPORT int mod_api_version = version
#define DECLARE_MOD_TYPE(type) \
		MODEXPORT int mod_type = type
#define DECLARE_MOD_NAME(name)	\
		MODEXPORT char mod_name[MODNAMELEN] = name

struct module;

/**
 * Prototype for mod_config() and mod_unconfig() functions
 */
typedef int (* mod_config_func)(struct module* mod);

/**
 * Return values for mod_config() and mod_unconfig() functions
 */
#define	MOD_OK			0
#define MOD_ERROR		-1

#endif /* MODAPI_H_ */
