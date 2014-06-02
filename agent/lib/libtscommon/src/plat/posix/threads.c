
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



#include <defs.h>
#include <plat/posix/threads.h>

#include <threads.h>

#include <unistd.h>
#include <sys/select.h>

PLATAPI void plat_thread_init(plat_thread_t* thread, void* arg,
		  	  	  	  	  	  thread_start_func start) {

	pthread_attr_init(&thread->t_attr);

	pthread_attr_setdetachstate(&thread->t_attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&thread->t_attr,TSTACKSIZE);

	pthread_create(&thread->t_thread,
			       &thread->t_attr,
			       start, arg);
}

PLATAPI void plat_thread_destroy(plat_thread_t* thread) {
	pthread_attr_destroy(&thread->t_attr);
	pthread_detach(thread->t_thread);
}

PLATAPI void plat_thread_join(plat_thread_t* thread) {
	pthread_join(thread->t_thread, NULL);
}

PLATAPI unsigned long plat_gettid() {
	return (unsigned long) pthread_self();
}

PLATAPI void t_eternal_wait(void) {
	select(0, NULL, NULL, NULL, NULL);
}

PLATAPI long t_get_pid(void) {
	return getpid();
}

