
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef MODAPI_H_
#define MODAPI_H_

#include <tsload/defs.h>


#define MODNAMELEN			    32

#define MOD_API_VERSION		0x0002

/**
 * Module types
 */
#define MOD_TSLOAD		1

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

