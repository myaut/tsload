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

/**
 * @module Network Sockets
 *
 * Platform-independent implementation for TCP/UDP network sockets.
 * Used by agent code and be useful for network clients loader agents.
 */

/**
 * @name Socket types
 *
 * @value NSK_STREAM	TCP socket
 * @value NSK_DRAM		UDP socket
 * */
#ifdef  TSDOC
#define NSK_STREAM
#define NSK_DRAM
#endif

/**
 * @name Return values
 *
 * Each constant represents failed operation
 * */
#define NSK_OK					0
#define NSK_ERR_RESOLVE			-1
#define NSK_ERR_SOCKET			-2
#define NSK_ERR_CONNECT			-3
#define NSK_ERR_BIND			-4
#define NSK_ERR_LISTEN			-5
#define NSK_ERR_ACCEPT			-6
#define NSK_ERR_SETOPT			-7
#define NSK_ERR_SETNB			-8

/**
 * @name nsk_poll() return values
 *
 * @value NSK_POLL_OK 			poll successful, no incoming data
 * @value NSK_POLL_NEW_DATA 	data was received
 * @value NSK_POLL_DISCONNECTED local or remote node issued disconnect
 * @value NSK_POLL_FAILURE 		poll call was failed
 * */
#define NSK_POLL_OK				0
#define NSK_POLL_NEW_DATA		1
#define NSK_POLL_DISCONNECT 	2
#define NSK_POLL_FAILURE		3

/**
 * @name nsk_recv() return values
 *
 * @value NSK_RECV_DISCONNECT	poll successful, no incoming data
 * @value NSK_RECV_NO_DATA 		no data was received (only for non-blocking sockets)
 * @value NSK_RECV_ERROR 		recv call was failed
 * */
#define NSK_RECV_DISCONNECT		-1
#define NSK_RECV_NO_DATA		-2
#define NSK_RECV_ERROR			-3

#define NSK_DEFAULT_BACKLOG		5

/**
 * Resolve hostname and write result into he
 *
 * @param host  hostname or string representation of IP
 * @param he	destination buffer
 *
 * @note On some platforms may lock mutexes, because implementation is not re-enterable
 */
LIBEXPORT PLATAPI int nsk_resolve(const char* host, nsk_host_entry* he);

/**
 * Set up socket address object
 *
 * @param sa	socket address object
 * @param he	host entry resolved in nsk_resolve()
 * @param port  port number
 */
LIBEXPORT PLATAPI int nsk_setaddr(nsk_addr* sa, nsk_host_entry* he, unsigned short port);

/**
 * Set up socket and connect to remote node
 *
 * @param clnt_socket socket object
 * @param sa  socket address of remote server
 * @param type socket type
 */
LIBEXPORT PLATAPI int nsk_connect(nsk_socket* clnt_socket, nsk_addr* sa, int type);

/**
 * Listen on server socket
 *
 * @param srv_socket socket object
 * @param sa  socket address of local server
 * @param type socket type
 */
LIBEXPORT PLATAPI int nsk_listen(nsk_socket* srv_socket, nsk_addr* sa, int type);

/**
 * Accept clients connection
 *
 * @param srv_socket server socket (earlier initialized with nsk_listen())
 * @param clnt_socket socket object (new object)
 * @param sa socket address of client
 */
LIBEXPORT PLATAPI int nsk_accept(nsk_socket* srv_socket, nsk_socket* clnt_socket, nsk_addr* clnt_sa);

/**
 * Shutdown and close connection - both for server and client sockets
 *
 * @param socket socket object
 */
LIBEXPORT PLATAPI int nsk_disconnect(nsk_socket* socket);

/**
 * Set socket option. Interface for platform setsockopt() or equal function.
 */
LIBEXPORT PLATAPI int nsk_setopt(nsk_socket* socket, int level, int optname,
								 const void* optval, size_t optlen);

/**
 * Enable or disable nonblocking operations on socket
 *
 * @param socket socket object
 * @param nonblocking if set to B_TRUE, enables non-blocking operations,\
 *   if set to B_FALSE - disables them
 */
LIBEXPORT PLATAPI int nsk_setnb(nsk_socket* socket, boolean_t nonblocking);

/**
 * Wait on incoming data or disconnect of socket.
 * Polling of multiple sockets is not supported
 *
 * @return One of NSK_POLL_* results
 */
LIBEXPORT PLATAPI int nsk_poll(nsk_socket* socket, ts_time_t timeout);

/**
 * Send data over socket
 *
 * @return 0 in case of disconnect, -1 on error, or number of bytes sent
 */
LIBEXPORT PLATAPI int nsk_send(nsk_socket* socket, void* data, size_t len);

/**
 * Receive data from socket
 *
 * @return one of NSK_RECV_* value
 */
LIBEXPORT PLATAPI int nsk_recv(nsk_socket* socket, void* data, size_t len);

LIBEXPORT PLATAPI int nsk_init(void);
LIBEXPORT PLATAPI void nsk_fini(void);


#endif /* NETSOCK_H_ */
