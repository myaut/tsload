/*
 * client.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#define LOG_SOURCE "client"
#include <log.h>

#include <defs.h>
#include <client.h>

#include <stdlib.h>

#include <plat/posix/client.h>

#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PLAT_LINUX
#define POLLRDHUP 0
#endif

PLATAPI plat_host_entry* plat_clnt_resolve(const char* host) {
	static struct hostent* he;
	int err = 0;
	struct in_addr addr;

	he = gethostbyname(host);

	if(he == NULL) {
		/*Fall back to gethostbyaddr*/
		if(inet_aton(host, &addr) == 0) {
			return NULL;
		}

		/*FIXME: IPv6 support*/
		he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	}

	return he;
}

PLATAPI int plat_clnt_setaddr(plat_clnt_addr* clnt_sa, plat_host_entry* he, int clnt_port) {
	if(he == NULL) {
		return CLNT_ERR_RESOLVE;
	}

	clnt_sa->sin_family = AF_INET;
	clnt_sa->sin_port = htons(clnt_port);
	clnt_sa->sin_addr = *((struct in_addr*) he->h_addr_list[0]);

	memset(clnt_sa->sin_zero, 0, sizeof(clnt_sa->sin_zero));

	return CLNT_OK;
}

PLATAPI int plat_clnt_connect(plat_clnt_socket* clnt_socket, plat_clnt_addr* clnt_sa) {
	*clnt_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(*clnt_socket == -1)
		return CLNT_ERR_SOCKET;

	if(connect(*clnt_socket, (struct sockaddr*) clnt_sa,
			   sizeof(struct sockaddr)) == -1)
		return CLNT_ERR_CONNECT;


	return CLNT_OK;
}

PLATAPI int plat_clnt_disconnect(plat_clnt_socket* clnt_socket) {
	shutdown(*clnt_socket, SHUT_RDWR);
	close(*clnt_socket);

	return CLNT_OK;
}

PLATAPI int plat_clnt_poll(plat_clnt_socket* clnt_socket, ts_time_t timeout) {
	struct pollfd sock_poll = {
		.fd = *clnt_socket,
		.events = POLLIN | POLLNVAL | POLLHUP | POLLRDHUP,
		.revents = 0
	};

	poll(&sock_poll, 1, timeout / T_MS);

	if(sock_poll.revents & POLLNVAL)
		return CLNT_POLL_FAILURE;

	if(sock_poll.revents & POLLHUP)
		return CLNT_POLL_DISCONNECT;

#ifdef PLAT_LINUX
	if(sock_poll.revents & POLLRDHUP)
		return CLNT_POLL_DISCONNECT;
#endif

	if(sock_poll.revents & POLLIN)
		return CLNT_POLL_NEW_DATA;

	return CLNT_POLL_OK;
}

PLATAPI int plat_clnt_send(plat_clnt_socket* clnt_socket, void* data, size_t len) {
	return send(*clnt_socket, data, len, 0);
}

PLATAPI int plat_clnt_recv(plat_clnt_socket* clnt_socket, void* data, size_t len) {
	return recv(*clnt_socket, data, len, 0);
}
