/*
 * netsock.c
 *
 *  Created on: 22.12.2012
 *      Author: myaut
 */

#define LOG_SOURCE "netsock"
#include <log.h>

#include <defs.h>
#include <netsock.h>

#include <stdlib.h>

#include <winsock2.h>

static void nsk_log_error(nsk_addr* sa, const char* what);

/**
 * NOTE: On Windows needs environment variable SYSTEMROOT to be defined
 * */
PLATAPI int nsk_resolve(const char* host, nsk_host_entry* he) {
	struct hostent* ohe;
	struct sockaddr_in sa;

	int err = 0;

	size_t sa_size = sizeof(sa);

	ohe = gethostbyname(host);

	if(ohe == NULL) {
		/* InetPton is supported only in Windows > 2008 or Vista,
		 * so we use WSAStringToAddress*/

		if(WSAStringToAddress(host, AF_INET, NULL, &sa, &sa_size) == SOCKET_ERROR)
			return NSK_ERR_RESOLVE;

		ohe = gethostbyaddr((const char*) &sa.sin_addr, sizeof(sa.sin_addr), AF_INET);
	}

	memcpy(he, ohe, sizeof(nsk_host_entry));

	return NSK_OK;
}

PLATAPI int nsk_setaddr(nsk_addr* sa, nsk_host_entry* he, int port) {
	if(he == NULL) {
		return NSK_ERR_RESOLVE;
	}

	sa->sin_family = AF_INET;
	sa->sin_port = htons(port);
	sa->sin_addr = *((struct in_addr*) he->h_addr_list[0]);

	memset(sa->sin_zero, 0, sizeof(sa->sin_zero));

	return NSK_OK;
}

PLATAPI int nsk_connect(nsk_socket* clnt_socket, nsk_addr* sa, int type) {
	*clnt_socket = socket(AF_INET, type, 0);

	if(*clnt_socket == -1) {
		nsk_log_error(sa, "socket");
		return NSK_ERR_SOCKET;
	}

	if(connect(*clnt_socket, (struct sockaddr*) sa,
			   sizeof(struct sockaddr)) != 0) {
		nsk_log_error(sa, "connect");
		closesocket(*clnt_socket);

		return NSK_ERR_CONNECT;
	}

	return NSK_OK;
}

PLATAPI int nsk_listen(nsk_socket* srv_socket, nsk_addr* sa, int type) {
	*srv_socket = socket(AF_INET, type, 0);

	if(*srv_socket == -1) {
		nsk_log_error(sa, "socket");
		return NSK_ERR_SOCKET;
	}

	if(bind(*srv_socket, sa, sizeof(nsk_addr)) != 0) {
		nsk_log_error(sa, "bind");
		closesocket(*srv_socket);
		return NSK_ERR_BIND;
	}

	if(listen(*srv_socket, NSK_DEFAULT_BACKLOG) != 0) {
		nsk_log_error(sa, "listen");
		closesocket(*srv_socket);
		return NSK_ERR_LISTEN;
	}

	return NSK_OK;
}

PLATAPI int nsk_accept(nsk_socket* srv_socket, nsk_socket* clnt_socket, nsk_addr* clnt_sa) {
	int addrlen = sizeof(nsk_addr);
	SOCKET s = accept(*srv_socket, clnt_sa, &addrlen);

	if(s == INVALID_SOCKET) {
		return NSK_ERR_ACCEPT;
	}

	*clnt_socket = s;
	return NSK_OK;
}

PLATAPI int nsk_disconnect(nsk_socket* socket) {
	shutdown(*socket, SD_BOTH);
	closesocket(*socket);

	return NSK_OK;
}

PLATAPI int nsk_setopt(nsk_socket* socket, int level, int optname,
								 const void* optval, size_t optlen) {
	if(setsockopt(*socket, level, optname, optval, optlen) != 0)
		return NSK_ERR_SETOPT;

	return NSK_OK;
}

PLATAPI int nsk_setnb(nsk_socket* socket, boolean_t nonblocking) {
	int arg = (nonblocking)? 1 : 0;

	if(ioctlsocket(*socket, FIONBIO, &arg) != 0) {
		return NSK_ERR_SETNB;
	}

	return NSK_OK;
}

PLATAPI int nsk_poll(nsk_socket* clnt_socket, ts_time_t timeout) {
	fd_set readfds;
	int ret;

	struct timeval tv_timeout;

	FD_ZERO(&readfds);
	FD_SET(*clnt_socket, &readfds);

	tv_timeout.tv_sec = timeout / T_SEC;
	tv_timeout.tv_usec = (timeout - tv_timeout.tv_sec * T_SEC) / T_US;

	if((ret = select(0, &readfds, NULL, NULL, &tv_timeout)) == SOCKET_ERROR)
		return NSK_POLL_FAILURE;

	if(ret > 0) {
		if(FD_ISSET(*clnt_socket, &readfds))
			return NSK_POLL_NEW_DATA;

		return NSK_POLL_DISCONNECT;
	}

	return NSK_POLL_OK;
}

PLATAPI int nsk_send(nsk_socket* socket, void* data, size_t len) {
	return send(*socket, data, len, 0);
}

PLATAPI int nsk_recv(nsk_socket* socket, void* data, size_t len) {
	int ret = recv(*socket, data, len, 0);
	int sockerr;

	if(ret > 0) {
			return ret;
	}
	else if(ret == 0) {
		return NSK_RECV_DISCONNECT;
	}

	sockerr = WSAGetLastError();
	if(sockerr == WSAEWOULDBLOCK) {
		return NSK_RECV_NO_DATA;
	}

	return NSK_RECV_ERROR;
}

PLATAPI	int nsk_init(void) {
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

PLATAPI	void nsk_fini(void) {
	WSACleanup();
}

static void nsk_log_error(nsk_addr* sa, const char* what) {
	char* ip = inet_ntoa(sa->sin_addr);

	logmsg(LOG_WARN, "%s to %s:%d failed: %d",
			what, ip, ntohs(sa->sin_port), WSAGetLastError());
}
