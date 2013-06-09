/*
 * genuuid.c
 *
 *  Created on: 09.06.2013
 *      Author: myaut
 */


#include <genuuid.h>

#include <string.h>

#include <rpc.h>

PLATAPI int generate_uuid(uuid_str_t uu) {
	UUID u;
	RPC_CSTR str;

	if(UuidCreateSequential(&u) != RPC_S_OK)
		return UUID_ERROR;

	if(UuidToString(&u, &str) != RPC_S_OK)
		return UUID_ERROR;

	strncpy(uu, str, UUIDSTRMAXLEN);

	RpcStringFree(&str);

	return UUID_OK;
}
