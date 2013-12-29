/*
 * netsock.h
 *
 *  Created on: Dec 29, 2012
 *      Author: myaut
 */

#ifndef PLAT_POSIX_NETSOCK_H_
#define PLAT_POSIX_NETSOCK_H_

#include <netdb.h>
#include <sys/socket.h>

typedef int nsk_socket;

typedef struct sockaddr_in nsk_addr;

typedef struct hostent nsk_host_entry;

#define NSK_STREAM		SOCK_STREAM
#define NSK_DRAM		SOCK_DGRAM

#endif /* PLAT_POSIX_NETSOCK_H_ */
