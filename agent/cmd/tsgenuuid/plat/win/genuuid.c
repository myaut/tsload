
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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

