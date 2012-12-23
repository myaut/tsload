/*
 * uname.c
 *
 *  Created on: Dec 17, 2012
 *      Author: myaut
 */

#include <defs.h>

#include <stdio.h>
#include <string.h>

#ifndef LSB_RELEASE_CMD
#define LSB_RELEASE_CMD "/usr/bin/lsb_release"
#endif

#define OSNAMELEN 	64

char lsb_os_name[64];
int lsb_read = B_FALSE;

/*Parses lsb_release output*/
void read_lsb_release(void) {
	FILE* pipe = popen(LSB_RELEASE_CMD " -d -s", "r");
	char descr_str[128] = "";
	char *p_descr = descr_str;
	size_t len;

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

	/*Remove last quote*/
	if(descr_str[len - 1] == '"')
		descr_str[len - 1] = 0;

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
