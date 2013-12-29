/*
 * netsock.h
 *
 *  Created on: Dec 29, 2012
 *      Author: myaut
 */

#ifndef PLAT_WIN_NETSOCK_H_
#define PLAT_WIN_NETSOCK_H_

#include <winsock2.h>

typedef SOCKET nsk_socket;

typedef struct sockaddr_in nsk_addr;

typedef struct hostent nsk_host_entry;

#define NSK_STREAM		SOCK_STREAM
#define NSK_DRAM		SOCK_DGRAM

#endif /* PLAT_WIN_NETSOCK_H_ */
