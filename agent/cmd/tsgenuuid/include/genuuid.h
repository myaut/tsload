/*
 * genuuid.h
 *
 *  Created on: 09.06.2013
 *      Author: myaut
 */

#ifndef GENUUID_H_
#define GENUUID_H_

#include <defs.h>

#define UUID_OK				0
#define UUID_ERROR			1

#define UUIDSTRMAXLEN		37

typedef char uuid_str_t[UUIDSTRMAXLEN];

PLATAPI int generate_uuid(uuid_str_t uu);

#endif
