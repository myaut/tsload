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

#include <winsock2.h>

PLATAPI plat_host_entry* plat_clnt_resolve(const char* host) {
	struct hostent* he;
	struct sockaddr_in sa;

	int err = 0;

	size_t sa_size = sizeof(sa);

	he = gethostbyname(host);

	if(he == NULL) {
		/* InetPton is supported only in Windows > 2008 or Vista,
		 * so we use WSAStringToAddress*/

		if(WSAStringToAddress(host, AF_INET, NULL, &sa, &sa_size) == SOCKET_ERROR)
			return NULL;

		return gethostbyaddr((const char*) &sa.sin_addr, sizeof(sa.sin_addr), AF_INET);
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

PLATAPI int plat_clnt_connect(plat_clnt_socket* clnt_socket, plat_clnt_addr* clnt_sa, int clnt_port) {
	*clnt_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(*clnt_socket == -1)
		return CLNT_ERR_SOCKET;

	if(connect(*clnt_socket, (struct sockaddr*) clnt_sa,
			   sizeof(struct sockaddr)) == -1)
		return CLNT_ERR_CONNECT;

	return CLNT_OK;
}

PLATAPI int plat_clnt_disconnect(plat_clnt_socket* clnt_socket) {
	shutdown(*clnt_socket, SD_BOTH);
	closesocket(*clnt_socket);

	return CLNT_OK;
}

PLATAPI int plat_clnt_poll(plat_clnt_socket* clnt_socket, ts_time_t timeout) {
	fd_set readfds;
	int ret;

	struct timeval tv_timeout;

	FD_ZERO(&readfds);
	FD_SET(*clnt_socket, &readfds);

	tv_timeout.tv_sec = timeout / T_SEC;
	tv_timeout.tv_usec = (timeout - tv_timeout.tv_sec * T_SEC) / T_US;

	if((ret = select(0, &readfds, NULL, NULL, &tv_timeout)) == SOCKET_ERROR)
		return CLNT_POLL_FAILURE;

	if(ret > 0) {
		if(FD_ISSET(*clnt_socket, &readfds))
			return CLNT_POLL_NEW_DATA;

		return CLNT_POLL_DISCONNECT;
	}

	return CLNT_POLL_OK;
}

PLATAPI int plat_clnt_send(plat_clnt_socket* clnt_socket, void* data, size_t len) {
	return send(*clnt_socket, data, len, 0);
}

PLATAPI int plat_clnt_recv(plat_clnt_socket* clnt_socket, void* data, size_t len) {
	return recv(*clnt_socket, data, len, 0);
}

PLATAPI	int plat_clnt_init(void) {
	WORD requested_ver;
	WSADATA wsa_data;
	int err;

	requested_ver = MAKEWORD(2, 0);

	err = WSAStartup(requested_ver, &wsa_data);

	if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 0) {
		WSACleanup();
		return -1;
	}

	return 0;
}

PLATAPI	void plat_clnt_fini(void) {
	WSACleanup();
}
