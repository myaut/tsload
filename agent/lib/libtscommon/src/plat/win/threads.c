/*
 * threads.c
 *
 *  Created on: 20.12.2012
 *      Author: myaut
 */

#include <defs.h>
#include <plat/win/threads.h>

#include <threads.h>

#include <windows.h>

#include <assert.h>

PLATAPI void plat_thread_init(plat_thread_t* thread, void* arg,
							  thread_start_func start) {

	thread->t_handle = CreateThread(NULL, TSTACKSIZE,
							        (LPTHREAD_START_ROUTINE) start,
							        arg,
							        0, NULL);

	assert(thread->t_handle != NULL);
}

PLATAPI void plat_thread_destroy(plat_thread_t* thread) {
	CloseHandle(thread->t_handle);
}

PLATAPI void plat_thread_join(plat_thread_t* thread) {
	WaitForSingleObject(thread->t_handle, INFINITE);
}

PLATAPI unsigned long plat_gettid() {
	return GetCurrentThreadId();
}

PLATAPI void t_eternal_wait(void) {
	Sleep(INFINITE);
}
