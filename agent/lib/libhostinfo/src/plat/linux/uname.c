/*
 * uname.c
 *
 *  Created on: Dec 17, 2012
 *      Author: myaut
 */

#include <defs.h>

#include <stdio.h>
#include <string.h>

#include <sys/utsname.h>

#ifndef LSB_RELEASE_PATH
#define LSB_RELEASE_PATH "/usr/bin/lsb_release"
#endif

#define OSNAMELEN 	64

char lsb_os_name[64];
boolean_t lsb_read = B_FALSE;

/* From posix implementation */
struct utsname* hi_get_uname();

/*Parses lsb_release output*/
void read_lsb_release(void) {
	FILE* pipe = popen(LSB_RELEASE_PATH " -d -s 2>/dev/null", "r");
	char descr_str[128] = "";
	char *p_descr = descr_str;
	size_t len;
	int tail;

	/* pipe() or fork() failed, failure! */
	if(pipe == NULL) {
		return;
	}

	fgets(descr_str, 128, pipe);
	len = strlen(descr_str);

	/*Not read release name, exit*/
	if(len == 0) {
		pclose(pipe);
		return;
	}

	/*Success, ignore first quotes*/
	if(*p_descr == '"')
		++p_descr;

	tail = len - 1;

	/*Remove trailing quotes and CRs*/
	while(descr_str[tail] == '"' ||
		  descr_str[tail] == '\n') {
		descr_str[tail] = '\0';
		--tail;
	}

	strncpy(lsb_os_name, p_descr, OSNAMELEN);

	pclose(pipe);
}

PLATAPI const char* hi_get_os_name() {
	if(!lsb_read) {
		strcpy(lsb_os_name, "Linux");
		read_lsb_release();
		lsb_read = B_TRUE;
	}

	return lsb_os_name;
}

PLATAPI const char* hi_get_domainname() {
	return hi_get_uname()->domainname;
}
