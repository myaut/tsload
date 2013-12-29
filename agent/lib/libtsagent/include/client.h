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

#define CLNT_RECV_TIMEOUT	1200 * T_MS

#define CLNT_CHUNK_SIZE		2048

#define CLNT_MAX_DISCONNECTS 5

#define CLNTHOSTLEN			32

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

#endif /* CLIENT_H_ */
