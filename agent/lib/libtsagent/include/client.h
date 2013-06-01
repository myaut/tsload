/*
 * client.h
 *
 *  Created on: 19.11.2012
 *      Author: myaut
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <libjson.h>

#include <defs.h>
#include <threads.h>
#include <tstime.h>

#include <plat/client.h>

#define CLNT_RECV_TIMEOUT	1200 * T_MS

#define CLNT_CHUNK_SIZE		2048

#define CLNT_MAX_DISCONNECTS 5

#define CLNTHOSTLEN			32

#define CLNT_OK				0
#define CLNT_ERR_RESOLVE	-1
#define CLNT_ERR_SOCKET		-2
#define CLNT_ERR_CONNECT	-3

#define CLNT_POLL_OK			0
#define CLNT_POLL_NEW_DATA		1
#define CLNT_POLL_DISCONNECT 	2
#define CLNT_POLL_FAILURE		3

typedef enum {
	RT_RESPONSE,
	RT_ERROR,

	RT_TIMEOUT,		/*Response was not received in timeout*/
	RT_DISCONNECT,  /*Disconnect during invokation*/

	RT_NOTHING
} clnt_response_type_t;

/* Handler for outgoing message */
typedef struct clnt_msg_handler {
	thread_event_t mh_event;

	unsigned mh_msg_id;

	struct clnt_msg_handler* mh_next;

	clnt_response_type_t mh_response_type;
	JSONNODE* mh_response;

	int mh_error_code;
} clnt_msg_handler_t;

/* Currently processing incoming message */
typedef struct clnt_proc_msg {
	unsigned m_msg_id;
	char* m_command;

	clnt_response_type_t m_response_type;
	JSONNODE* m_response;
} clnt_proc_msg_t;

#define CLNTMHTABLESIZE 16
#define CLNTMHTABLEMASK (CLNTMHTABLESIZE - 1)

#define CLNT_RETRY_TIMEOUT (3ll * T_SEC)

LIBEXPORT clnt_response_type_t clnt_invoke(const char* command, JSONNODE* msg_node, JSONNODE** p_response);
clnt_proc_msg_t* clnt_proc_get_msg();
void clnt_add_response(clnt_response_type_t type, JSONNODE* node);

LIBEXPORT boolean_t clnt_proc_error(void);

LIBEXPORT int clnt_init(void);
LIBEXPORT void clnt_fini(void);

PLATAPI	int plat_clnt_init(void);
PLATAPI	void plat_clnt_fini(void);

PLATAPI plat_host_entry* plat_clnt_resolve(const char* host);
PLATAPI int plat_clnt_setaddr(plat_clnt_addr* clnt_sa, plat_host_entry* he, int clnt_port);

PLATAPI int plat_clnt_connect(plat_clnt_socket* clnt_socket, plat_clnt_addr* clnt_sa);
PLATAPI int plat_clnt_disconnect(plat_clnt_socket* clnt_socket);

PLATAPI int plat_clnt_poll(plat_clnt_socket* clnt_socket, ts_time_t timeout);

PLATAPI int plat_clnt_send(plat_clnt_socket* clnt_socket, void* data, size_t len);
PLATAPI int plat_clnt_recv(plat_clnt_socket* clnt_socket, void* data, size_t len);

#endif /* CLIENT_H_ */
