/*
 * genuuid.c
 *
 *  Created on: 09.06.2013
 *      Author: myaut
 */


#include <genuuid.h>

#include <uuid/uuid.h>

PLATAPI int generate_uuid(uuid_str_t uu) {
	uuid_t u;

	uuid_generate_time(u);
	uuid_unparse(u, uu);

	return UUID_OK;
}
