#define LOG_SOURCE "client"
#include <tsload/log.h>

#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/hashmap.h>
#include <tsload/syncqueue.h>
#include <tsload/atomic.h>
#include <tsload/time.h>

#include <tsload/agent/agent.h>
#include <tsload/agent/client.h>

#include <stdlib.h>
#include <assert.h>

#include <libjson.h>


#define CLNT_CHUNK	1024

JSONNODE* json_clnt_command_format(const char* command, JSONNODE* msg_node, unsigned msg_id);
void clnt_on_disconnect(void);
int clnt_send(JSONNODE* node);

/* Globals */
nsk_socket clnt_socket;
nsk_addr	 clnt_addr;

LIBEXPORT int clnt_port = 9090;
LIBEXPORT char clnt_host[CLNTHOSTLEN] = "localhost";

extern int agent_id;

boolean_t clnt_trace_decode = B_FALSE;

/* Message unique identifier */
static atomic_t clnt_msg_id = 0;

/* Threads and it's flags */
boolean_t clnt_connected = B_FALSE;
boolean_t clnt_finished = B_FALSE;

int clnt_disconnects = 0;

thread_t t_client_receive;
thread_t t_client_connect;
thread_t t_client_process;

/* Protects sending from multiple threads simultaneously*/
static thread_mutex_t send_mutex;

static thread_event_t conn_event;

squeue_t proc_queue;

mp_cache_t hdl_cache;

thread_key_t	proc_msg_key;

/*
 * Hash table helpers
 * */
DECLARE_HASH_MAP(hdl_hashmap, B_FALSE, clnt_msg_handler_t, CLNTMHTABLESIZE, mh_msg_id, mh_next,
	{
		unsigned* msg_id = (unsigned*) key;
		return *msg_id & CLNTMHTABLEMASK;
	},
	{
		unsigned* msg_id1 = (unsigned*) key1;
		unsigned* msg_id2 = (unsigned*) key2;

		return *msg_id1 == *msg_id2;
	}
);

/*
 * Should be called from clnt_proc_thread
 *
 * @return message that is currently processed
 * */
clnt_proc_msg_t* clnt_proc_get_msg(void) {
	return (clnt_proc_msg_t*) tkey_get(&proc_msg_key);
}

void clnt_proc_set_msg(clnt_proc_msg_t* msg) {
	tkey_set(&proc_msg_key, msg);
}

static clnt_msg_handler_t* clnt_create_msg() {
	unsigned msg_id = (unsigned) atomic_inc(&clnt_msg_id);
	clnt_msg_handler_t* hdl = (clnt_msg_handler_t*) mp_cache_alloc(&hdl_cache);

	event_init(&hdl->mh_event, "clnt_msg_handler");

	hdl->mh_next = NULL;
	hdl->mh_msg_id = msg_id;
	hdl->mh_response = NULL;
	hdl->mh_response_type = RT_NOTHING;
	hdl->mh_error_code = 0;

	hash_map_insert(&hdl_hashmap, hdl);

	return hdl;
}

static void clnt_delete_handler(clnt_msg_handler_t* hdl) {
	hash_map_remove(&hdl_hashmap, hdl);

	mp_cache_free(&hdl_cache, hdl);
}

int clnt_process_response(unsigned msg_id, JSONNODE* response, clnt_response_type_t rt, JSONNODE* n_code) {
	clnt_msg_handler_t* hdl = NULL;
	int error_code;

	hdl = (clnt_msg_handler_t*) hash_map_find(&hdl_hashmap, &msg_id);

	if(hdl == NULL) {
		logmsg(LOG_WARN, "Failed to process incoming message: unknown message id %u", msg_id);
		json_delete(response);
		return -1;
	}

	if(n_code != NULL) {
		hdl->mh_error_code = json_as_int(n_code);
	}

	/*Notify sender thread that we got a response*/
	hdl->mh_response_type = rt;
	hdl->mh_response = json_copy(response);

	event_notify_one(&hdl->mh_event);

	return 0;
}

/**
 * Process incoming command
 *
 * @return 0 if all OK or -1 if message was incorrect
 */
int clnt_process_command(unsigned msg_id, JSONNODE* n_cmd, JSONNODE* n_msg) {
	clnt_proc_msg_t* msg;

	if(json_type(n_cmd) != JSON_STRING ||
	   json_type(n_msg) != JSON_NODE)
		return -1;

	/* Create proc message */
	msg = (clnt_proc_msg_t*) mp_malloc(sizeof(clnt_proc_msg_t));

	msg->m_msg_id = msg_id;
	msg->m_command = json_as_string(n_cmd);
	msg->m_response_type = RT_NOTHING;
	msg->m_response = NULL;

	clnt_proc_set_msg(msg);

	/* Call agent's method */

	logmsg(LOG_TRACE, "Invoked command %s message #%d", msg->m_command, msg_id);

	agent_process_command(msg->m_command, n_msg);

	/* Process response */

	assert(msg->m_response != NULL);

	json_push_back(msg->m_response, json_new_i("id", msg_id));
	clnt_send(msg->m_response);

	/* Clean up */

	json_delete(msg->m_response);
	json_free(msg->m_command);

	clnt_proc_set_msg(NULL);
	mp_free(msg);

	return 0;
}

/* Process incoming message from buffer
 *
 * - Parses buffer into JSON
 * - Finds 'id' field in JSON
 * - Searches handler with appropriate id
 * - Unblocks sender, and sets response JSON
 *
 * @return 0 if processing successful and -1 otherwise*/
int clnt_process_msg(JSONNODE* msg) {
	JSONNODE_ITERATOR i_id, i_agentid, i_response, i_command, i_msg, i_error, i_code, i_end;
	unsigned msg_id;
	int ret = 0;
	int msg_agent_id;

	if(msg == NULL) {
		logmsg(LOG_WARN, "Failed to process incoming message: not JSON");
		ret = -1;
		goto fail;
	}

	i_end = json_end(msg);
	i_id = json_find(msg, "id");
	i_agentid = json_find(msg, "agentId");

	if(i_id == i_end || json_type(*i_id) != JSON_NUMBER) {
		logmsg(LOG_WARN, "Failed to process incoming message: unknown 'id'");
		ret = -1;
		goto fail;
	}

	if(i_agentid == i_end || json_type(*i_agentid) != JSON_NUMBER) {
		logmsg(LOG_WARN, "Failed to process incoming message: unknown 'agentId'");
		ret = -1;
		goto fail;
	}

	msg_agent_id = json_as_int(*i_agentid);

	if(agent_id > 0 && msg_agent_id != agent_id) {
		logmsg(LOG_WARN, "Failed to process incoming message: invalid 'agentId': ours is %d, got %d",
				agent_id, msg_agent_id);
		ret = -1;
		goto fail;
	}

	msg_id = json_as_int(*i_id);

	/* Determine, which type of message we received and
	* go to subroutine*/
	if((i_response = json_find(msg, "response")) != i_end) {
		ret = clnt_process_response(msg_id, *i_response, RT_RESPONSE, NULL);
	} else if(
			(i_command = json_find(msg, "cmd")) != i_end &&
			(i_msg = json_find(msg, "msg")) != i_end) {
		ret = clnt_process_command(msg_id, *i_command, *i_msg);
	}
	else if((i_code = json_find(msg, "code")) != i_end &&
			(i_error = json_find(msg, "error")) != i_end) {
		ret = clnt_process_response(msg_id, *i_error, RT_ERROR, *i_code);
	}
	else {
		logmsg(LOG_WARN, "Failed to process incoming message: invalid format");
		ret = -1;
	}

fail:
	if(msg)
		json_delete(msg);

	return ret;
}

thread_result_t clnt_proc_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);
	JSONNODE* msg;

	while(!clnt_finished) {
		msg = (JSONNODE*) squeue_pop(&proc_queue);

		if(msg == NULL) {
			THREAD_EXIT(0);
		}

		clnt_process_msg(msg);
	}

	THREAD_END:
		logmsg(LOG_WARN, "clnt_proc_thread thread is dead!");
		THREAD_FINISH(arg);
}

void clnt_recv_parse(void* buffer, size_t recvlen) {
	JSONNODE* node;
	char* msg = (char*) buffer;

	size_t off = 0, len = 0;

	while(off < recvlen) {
		/*Process data in buffer*/
		len = strlen(msg) + 1;

		if(clnt_trace_decode)
			logmsg(LOG_TRACE, "Processing %lu bytes off: %lu", len, off);

		node = json_parse(msg);

		logmsg(LOG_TRACE, "IN msg: %s", msg);

		if(node != NULL) {
			squeue_push(&proc_queue, node);
		}
		else {
			logmsg(LOG_WARN, "Failure during receive: not a valid JSON");
		}

		off += len;
		msg += len;
	}
}

/**
 * clnt_recv_thread - thread that receives data from socket,
 * parses it and sends to clnt_process_thread
 */
thread_result_t clnt_recv_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);

	char *buffer, *bufptr;
	size_t buflen;
	size_t recvlen;

	size_t remaining;

	int ret;

	while(!clnt_finished && clnt_connected) {
		switch(nsk_poll(&clnt_socket, CLNT_RECV_TIMEOUT)) {
		case NSK_POLL_OK:
			/*No new data arrived*/
			continue;
		case NSK_POLL_FAILURE:
			logmsg(LOG_CRIT, "Failure while polling socket");
			THREAD_EXIT(-1);
		case NSK_POLL_DISCONNECT:
			logmsg(LOG_CRIT, "Disconnected while polling socket");
			THREAD_EXIT(-1);
		}
		/*Everything ok - new data arrived*/

		recvlen = 0;
		remaining = buflen = CLNT_CHUNK_SIZE;
		bufptr = buffer = mp_malloc(buflen);
		memset(buffer, 0, buflen);

		/*Receive data from socket by chunks of data*/
		while(1) {
			/*Receive chunk of data*/
			ret = nsk_recv(&clnt_socket, bufptr, remaining);

			if(ret <= 0) {
				logmsg(LOG_WARN, "recv() was failed, disconnecting");
				THREAD_EXIT(-1);
			}

			bufptr += ret;
			recvlen += ret;
			remaining -= ret;

			/*No more data left in socket, break*/
			if(nsk_poll(&clnt_socket, 0l) != NSK_POLL_NEW_DATA)
				break;

			/* We had filled buffer, reallocate it */
			if(remaining == 0) {
				buflen += CLNT_CHUNK_SIZE;
				buffer = mp_realloc(buffer, buflen);

				remaining += CLNT_CHUNK_SIZE;

				memset(bufptr, 0, CLNT_CHUNK_SIZE);
			}
		}

		if(clnt_trace_decode)
			logmsg(LOG_TRACE, "Received buffer @%p len: %lu", buffer, recvlen);

		clnt_recv_parse(buffer, recvlen);

		mp_free(buffer);
		buffer = NULL;
	}

THREAD_END:
	clnt_on_disconnect();
	mp_free(buffer);

	THREAD_FINISH(arg);
}

/* Returns B_TRUE if error was reported during processing of
 * this message */
boolean_t clnt_proc_error(void) {
	clnt_proc_msg_t* msg = clnt_proc_get_msg();

	return msg->m_response_type == RT_ERROR;
}

/* Adds response to command that is processed
 * Called by agent and saves response message (error or message)
 * to clnt_proc_thread.t_arg
 *
 * @param node response or error
 *
 * @see agent_response_msg, @see agent_error_msg
 * */
void clnt_add_response(clnt_response_type_t type, JSONNODE* node) {
	clnt_proc_msg_t* msg = clnt_proc_get_msg();

	assert(node != NULL);
	assert(msg != NULL);

	if(msg->m_response != NULL) {
		/* Already has a response, discard this one.
		 *
		 * This may happen if callee already reports error,
		 * than caller reports an internal error just to
		 * be sure */
		json_delete(node);
		return;
	}

	msg->m_response_type = type;
	msg->m_response = node;
}

/**
 * Send JSON message to remote server
 *
 * @param node - message
 *
 * @return number of sent bytes or -1 on error
 */
int clnt_send(JSONNODE* node) {
	char* json_msg = NULL;
	size_t len = 0;

	int ret;

	if(!clnt_connected) {
		logmsg(LOG_CRIT, "Failed to send message, client not connected");

		return -1;
	}

	json_push_back(node, json_new_i("agentId", agent_id));

	json_msg = json_write(node);
	len = strlen(json_msg) + 1;

	logmsg(LOG_TRACE, "OUT msg: %s", json_msg);

	mutex_lock(&send_mutex);
	ret = nsk_send(&clnt_socket, json_msg, len);
	mutex_unlock(&send_mutex);

	json_free(json_msg);

	return ret;
}

/*
 * Invoke command on remote server
 *
 * @param command - command name
 * @param msg_node - JSON representation of command'params
 * @param p_response - pointer where response is written
 *
 * NOTE: deletes msg_node in any case
 * */
clnt_response_type_t clnt_invoke(const char* command, JSONNODE* msg_node, JSONNODE** p_response) {
	clnt_msg_handler_t* hdl = NULL;
	clnt_response_type_t response_type = RT_NOTHING;
	JSONNODE* node = NULL;

	if(!clnt_connected) {
		json_delete(msg_node);
		return RT_DISCONNECT;
	}

	hdl = clnt_create_msg();
	if(hdl == NULL) {
		json_delete(msg_node);
		return RT_NOTHING;
	}

	node = json_clnt_command_format(command, msg_node, hdl->mh_msg_id);

	clnt_send(node);

	/*Wait until response will arrive*/
	event_wait(&hdl->mh_event);

	*p_response = hdl->mh_response;
	response_type = hdl->mh_response_type;

	/*Delete handler because it is not needed anymore*/
	clnt_delete_handler(hdl);
	json_delete(node);

	return response_type;
}

PLATAPI int clnt_connect() {
	int err;

	err = nsk_connect(&clnt_socket, &clnt_addr, NSK_STREAM);

	switch(err) {
		case NSK_ERR_SOCKET:
			logmsg(LOG_CRIT, "Failed to open socket");
			return err;
		case NSK_ERR_CONNECT:
			logmsg(LOG_CRIT, "Failed to connect to %s:%d, retrying in %lds", clnt_host, clnt_port,
						(long) (CLNT_RETRY_TIMEOUT / T_SEC));
			return err;
	}

	logmsg(LOG_INFO, "Connected to %s:%d", clnt_host, clnt_port);

	return err;
}

thread_result_t clnt_connect_thread(thread_arg_t arg) {
	THREAD_ENTRY(arg, void, unused);

	while(!clnt_finished) {
		if(!clnt_connected) {
			agent_id = 0;

			if(clnt_connect() != NSK_OK) {
				tm_sleep_milli(CLNT_RETRY_TIMEOUT);
				continue;
			}

			clnt_connected = B_TRUE;

			/*Restart receive thread*/
			t_init(&t_client_receive, NULL, clnt_recv_thread, "clnt_recv_thread");

			if(agent_hello() == -1) {
				/* Failed to register on server */
				exit(1);
			}
		}

		/*Wait until socket will be disconnected*/
		if(clnt_connected)
			event_wait(&conn_event);
	}

THREAD_END:
	THREAD_FINISH(arg);
}

int clnt_handler_disconnect(hm_item_t* object, void* arg) {
	clnt_msg_handler_t* hdl = (clnt_msg_handler_t*) object;

	logmsg(LOG_WARN, "Failed to send message #%u", hdl->mh_msg_id);

	hdl->mh_response_type = RT_DISCONNECT;
	event_notify_one(&hdl->mh_event);

	return HM_WALKER_CONTINUE;
}

int clnt_handler_cleanup(hm_item_t* object, void* arg) {
	clnt_msg_handler_t* hdl = (clnt_msg_handler_t*) object;

	hdl->mh_response_type = RT_NOTHING;
	event_notify_one(&hdl->mh_event);

	return HM_WALKER_CONTINUE | HM_WALKER_REMOVE;
}

void clnt_on_disconnect(void) {
	clnt_connected = B_FALSE;

	hash_map_walk(&hdl_hashmap, clnt_handler_disconnect, NULL);

	nsk_disconnect(&clnt_socket);

	/*Notify connect thread*/
	event_notify_all(&conn_event);

	if(++clnt_disconnects > CLNT_MAX_DISCONNECTS) {
		logmsg(LOG_CRIT, "Too much client disconnections; abort");
		abort();
	}
}

int clnt_init() {
	nsk_host_entry he;

	if(nsk_resolve(clnt_host, &he) == NSK_ERR_RESOLVE ||
	   nsk_setaddr(&clnt_addr, &he, clnt_port) == NSK_ERR_RESOLVE) {
		logmsg(LOG_CRIT, "Failed to resolve host %s", clnt_host);
		return -1;
	}

	mp_cache_init(&hdl_cache, clnt_msg_handler_t);

	hash_map_init(&hdl_hashmap, "hdl_hashmap");
	mutex_init(&send_mutex, "send_mutex");
	event_init(&conn_event, "conn_event");

	tkey_init(&proc_msg_key, "proc_msg_key");

	squeue_init(&proc_queue, "proc_queue");

	t_init(&t_client_connect, NULL, clnt_connect_thread, "clnt_conn_thread");
	t_init(&t_client_process, NULL, clnt_proc_thread, "clnt_proc_thread");

	return 0;
}

void clnt_fini(void) {
	clnt_finished = B_TRUE;
	clnt_connected = B_FALSE;

	/*This will finish conn_thread*/
	clnt_on_disconnect();

	/*Delete all remaining messages and finish proc_thread*/
	squeue_destroy(&proc_queue, json_delete);

	/*Finish connect thread*/
	event_notify_all(&conn_event);

	t_destroy(&t_client_process);
	t_destroy(&t_client_receive);
	t_destroy(&t_client_connect);

	tkey_destroy(&proc_msg_key);

	event_destroy(&conn_event);
	mutex_destroy(&send_mutex);

	hash_map_walk(&hdl_hashmap, clnt_handler_cleanup, NULL);
	hash_map_destroy(&hdl_hashmap);

	mp_cache_destroy(&hdl_cache);
}

JSONNODE* json_clnt_command_format(const char* command, JSONNODE* msg_node, unsigned msg_id) {
	JSONNODE* node = json_new(JSON_NODE);

	json_push_back(node, json_new_i("id", msg_id));
	json_push_back(node, json_new_a("cmd", command));

	json_set_name(msg_node, "msg");
	json_push_back(node, msg_node);

	return node;
}

