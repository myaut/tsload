
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

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



#include <tsload/defs.h>

#include <tsload/netsock.h>
#include <tsload/threads.h>

#include <assert.h>


#define BUFSIZE		8
#define MESSAGE		"Hello!"
#define MSGLEN		6

int port = 9091;

thread_result_t client_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	nsk_host_entry he;
	nsk_socket socket;
	nsk_addr srv_sa;

	char buffer[BUFSIZE];

	assert(nsk_resolve("localhost", &he) == NSK_OK);
	assert(nsk_setaddr(&srv_sa, &he, port) == NSK_OK);
	assert(nsk_connect(&socket, &srv_sa, SOCK_STREAM) == NSK_OK);

	assert(nsk_send(&socket, MESSAGE, MSGLEN) == MSGLEN);
	assert(nsk_recv(&socket, buffer, BUFSIZE) == MSGLEN);

	nsk_disconnect(&socket);

THREAD_END:
	THREAD_FINISH(arg);
}

int test_main() {
	nsk_host_entry he;
	nsk_addr sa;
	nsk_addr clnt_sa;
	nsk_socket srv_socket;
	nsk_socket clnt_socket;

	thread_t client;

	char buffer[BUFSIZE];

	threads_init();
	nsk_init();

	assert(nsk_resolve("0.0.0.0", &he) == NSK_OK);
	assert(nsk_setaddr(&sa, &he, port) == NSK_OK);
	assert(nsk_listen(&srv_socket, &sa, SOCK_STREAM) == NSK_OK);

	t_init(&client, NULL, client_thread, "client_thread");

	assert(nsk_accept(&srv_socket, &clnt_socket, &clnt_sa) == NSK_OK);
	assert(nsk_recv(&clnt_socket, buffer, BUFSIZE) == MSGLEN);
	assert(nsk_send(&clnt_socket, MESSAGE, MSGLEN) == MSGLEN);

	t_join(&client);
	t_destroy(&client);

	nsk_disconnect(&clnt_socket);
	nsk_disconnect(&srv_socket);

	nsk_fini();
	threads_fini();

	return 0;
}

