/*
 * client.h
 *
 *  Created on: Dec 29, 2012
 *      Author: myaut
 */

#ifndef PLAT_POSIX_CLIENT_H_
#define PLAT_POSIX_CLIENT_H_

#include <netdb.h>
#include <sys/socket.h>

typedef int plat_clnt_socket;

typedef struct sockaddr_in plat_clnt_addr;

typedef struct hostent plat_host_entry;

#endif /* CLIENT_H_ */
