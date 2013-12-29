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
#include <threads.h>

#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

static void nsk_log_error(nsk_addr* sa, const char* what);

#ifndef PLAT_LINUX
#define POLLRDHUP 0
#endif

/*
 * Since resolver functions are non-reenterable, implement a mutex
 * TODO: Use _r functions i.e. on linux
 *  */
static thread_mutex_t nsk_resolver_mutex;

PLATAPI int nsk_resolve(const char* host, nsk_host_entry* he) {
	static struct hostent* ohe;
	int err = 0;
	struct in_addr addr;

	mutex_lock(&nsk_resolver_mutex);
	ohe = gethostbyname(host);

	if(ohe == NULL) {
		/*Fall back to gethostbyaddr*/
		if(inet_aton(host, &addr) == 0) {
			mutex_unlock(&nsk_resolver_mutex);
			return NSK_ERR_RESOLVE;
		}

		/*FIXME: IPv6 support*/
		ohe = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	}

	memcpy(he, ohe, sizeof(nsk_host_entry));
	mutex_unlock(&nsk_resolver_mutex);

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
		close(*clnt_socket);

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
		close(*srv_socket);
		return NSK_ERR_BIND;
	}

	if(listen(*srv_socket, NSK_DEFAULT_BACKLOG) != 0) {
		nsk_log_error(sa, "listen");
		close(*srv_socket);
		return NSK_ERR_LISTEN;
	}

	return NSK_OK;
}

PLATAPI int nsk_accept(nsk_socket* srv_socket, nsk_socket* clnt_socket, nsk_addr* clnt_sa) {
	socklen_t len = sizeof(nsk_addr);
	int sd = accept(*srv_socket, clnt_sa, &len);

	if(sd == -1) {
		return NSK_ERR_ACCEPT;
	}

	*clnt_socket = sd;
	return NSK_OK;
}

PLATAPI int nsk_disconnect(nsk_socket* socket) {
	shutdown(*socket, SHUT_RDWR);
	close(*socket);

	return NSK_OK;
}

PLATAPI int nsk_setopt(nsk_socket* socket, int level, int optname,
								 const void* optval, size_t optlen) {
	if(setsockopt(*socket, level, optname, optval, optlen) != 0)
		return NSK_ERR_SETOPT;

	return NSK_OK;
}

PLATAPI int nsk_setnb(nsk_socket* socket, boolean_t nonblocking) {
	int flags = fcntl(*socket, F_GETFL, 0);

	if(flags == -1)
		return NSK_ERR_SETNB;

	if(nonblocking) {
		flags |= O_NONBLOCK;
	}
	else {
		flags &= ~O_NONBLOCK;
	}

	fcntl(*socket, F_SETFL, flags);

	return NSK_OK;
}

PLATAPI int nsk_poll(nsk_socket* clnt_socket, ts_time_t timeout) {
	struct pollfd sock_poll = {
		.fd = *clnt_socket,
		.events = POLLIN | POLLNVAL | POLLHUP | POLLRDHUP,
		.revents = 0
	};

	poll(&sock_poll, 1, timeout / T_MS);

	if(sock_poll.revents & POLLNVAL)
		return NSK_POLL_FAILURE;

	if(sock_poll.revents & POLLHUP)
		return NSK_POLL_DISCONNECT;

#ifdef PLAT_LINUX
	if(sock_poll.revents & POLLRDHUP)
		return NSK_POLL_DISCONNECT;
#endif

	if(sock_poll.revents & POLLIN)
		return NSK_POLL_NEW_DATA;

	return NSK_POLL_OK;
}

PLATAPI int nsk_send(nsk_socket* socket, void* data, size_t len) {
	return send(*socket, data, len, 0);
}

PLATAPI int nsk_recv(nsk_socket* socket, void* data, size_t len) {
	int ret = recv(*socket, data, len, 0);

	if(ret > 0) {
		return ret;
	}
	else if(ret == 0) {
		return NSK_RECV_DISCONNECT;
	}

	if(errno == EAGAIN || errno == EWOULDBLOCK) {
		return NSK_RECV_NO_DATA;
	}

	return NSK_RECV_ERROR;
}

PLATAPI	int nsk_init(void) {
	mutex_init(&nsk_resolver_mutex, "nsk_resolver_mtx");

	return 0;
}

PLATAPI	void nsk_fini(void) {
	mutex_destroy(&nsk_resolver_mutex);
}

static void nsk_log_error(nsk_addr* sa, const char* what) {
	char* ip = inet_ntoa(sa->sin_addr);

	logmsg(LOG_WARN, "%s to %s:%d failed: %s",
			what, ip, ntohs(sa->sin_port), strerror(errno));
}
