/*
 * netsock.h
 *
 *  Created on: Dec 29, 2013
 *      Author: myaut
 */

#ifndef NETSOCK_H_
#define NETSOCK_H_

#include <defs.h>
#include <tstime.h>
#include <plat/netsock.h>

#define NSK_OK					0
#define NSK_ERR_RESOLVE			-1
#define NSK_ERR_SOCKET			-2
#define NSK_ERR_CONNECT			-3
#define NSK_ERR_BIND			-4
#define NSK_ERR_LISTEN			-5
#define NSK_ERR_ACCEPT			-6
#define NSK_ERR_SETOPT			-7
#define NSK_ERR_SETNB			-8

#define NSK_POLL_OK				0
#define NSK_POLL_NEW_DATA		1
#define NSK_POLL_DISCONNECT 	2
#define NSK_POLL_FAILURE		3

#define NSK_RECV_DISCONNECT		-1
#define NSK_RECV_NO_DATA		-2
#define NSK_RECV_ERROR			-2

#define NSK_DEFAULT_BACKLOG		5

LIBEXPORT PLATAPI int nsk_resolve(const char* host, nsk_host_entry* he);
LIBEXPORT PLATAPI int nsk_setaddr(nsk_addr* sa, nsk_host_entry* he, int port);

LIBEXPORT PLATAPI int nsk_connect(nsk_socket* clnt_socket, nsk_addr* sa, int type);
LIBEXPORT PLATAPI int nsk_listen(nsk_socket* srv_socket, nsk_addr* sa, int type);
LIBEXPORT PLATAPI int nsk_accept(nsk_socket* srv_socket, nsk_socket* clnt_socket, nsk_addr* clnt_sa);
LIBEXPORT PLATAPI int nsk_disconnect(nsk_socket* socket);

LIBEXPORT PLATAPI int nsk_setopt(nsk_socket* socket, int level, int optname,
								 const void* optval, size_t optlen);
LIBEXPORT PLATAPI int nsk_setnb(nsk_socket* socket, boolean_t nonblocking);

LIBEXPORT PLATAPI int nsk_poll(nsk_socket* clnt_socket, ts_time_t timeout);

LIBEXPORT PLATAPI int nsk_send(nsk_socket* socket, void* data, size_t len);
LIBEXPORT PLATAPI int nsk_recv(nsk_socket* socket, void* data, size_t len);

LIBEXPORT PLATAPI int nsk_init(void);
LIBEXPORT PLATAPI void nsk_fini(void);


#endif /* NETSOCK_H_ */
