
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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

#include <tsload/threads.h>

#include <assert.h>

#include <windows.h>


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

PLATAPI long t_get_pid(void) {
	return GetCurrentProcessId();
}

