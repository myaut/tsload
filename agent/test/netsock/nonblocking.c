/*
 * netsock.c
 *
 *  Created on: Dec 29, 2013
 *      Author: myaut
 */

#include <threads.h>
#include <netsock.h>

#include <assert.h>

#define BUFSIZE		8

int port = 9091;

thread_result_t client_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	nsk_host_entry he;
	nsk_socket socket;
	nsk_addr srv_sa;

	char buffer[BUFSIZE];

	int ret;

	assert(nsk_resolve("localhost", &he) == NSK_OK);
	assert(nsk_setaddr(&srv_sa, &he, port) == NSK_OK);
	assert(nsk_connect(&socket, &srv_sa, SOCK_STREAM) == NSK_OK);
	assert(nsk_setnb(&socket, B_TRUE) == NSK_OK);

	assert(nsk_recv(&socket, buffer, BUFSIZE) == NSK_RECV_NO_DATA);

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

	t_join(&client);
	t_destroy(&client);

	nsk_disconnect(&clnt_socket);
	nsk_disconnect(&srv_socket);

	nsk_fini();
	threads_fini();

	return 0;
}