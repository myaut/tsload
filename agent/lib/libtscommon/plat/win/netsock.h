
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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



#ifndef PLAT_WIN_NETSOCK_H_
#define PLAT_WIN_NETSOCK_H_

#include <tsload/defs.h>

/* This prevents complaints about _WINSOCKAPI_ redefinition, because it is defined 
   by plat to prevent WinSock inclusion by windows.h */
#undef _WINSOCKAPI_
#include <winsock2.h>

typedef SOCKET nsk_socket;

typedef struct sockaddr_in nsk_addr;

typedef struct hostent nsk_host_entry;

#define NSK_STREAM		SOCK_STREAM
#define NSK_DRAM		SOCK_DGRAM

#endif /* PLAT_WIN_NETSOCK_H_ */

