/*
 * uname.c
 *
 *  Created on: Dec 17, 2012
 *      Author: myaut
 */

#include <defs.h>

#include <plat/sysfs.h>

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/utsname.h>

#define SYS_DMI_PATH		"/sys/class/dmi/id/"

#ifndef LSB_RELEASE_PATH
#define LSB_RELEASE_PATH "/usr/bin/lsb_release"
#endif

#define OSNAME_OK		0
#define OSNAME_ERROR	1

#define OSNAMELEN 		64
#define SYSNAMELEN		128

char hi_linux_os_name[OSNAMELEN];
boolean_t hi_linux_release_read = B_FALSE;

char hi_linux_sys_name[SYSNAMELEN];
boolean_t hi_linux_sysname_found = B_FALSE;

/* From posix implementation */
struct utsname* hi_get_uname();

static int read_lsb_release(void);
static int read_redhat_release(void);
static int read_suse_release(void);

PLATAPI const char* hi_get_os_name() {
	if(!hi_linux_release_read) {
		if(read_redhat_release() != OSNAME_OK &&
		   read_suse_release() != OSNAME_OK &&
		   read_lsb_release() != OSNAME_OK) {
			strcpy(hi_linux_os_name, "Unknown Linux");
		}

		hi_linux_release_read = B_TRUE;
	}

	return hi_linux_os_name;
}

PLATAPI const char* hi_get_domainname() {
	return hi_get_uname()->domainname;
}

PLATAPI const char* hi_get_sys_name() {
	char vendor[64] = "";
	char model[128] = "Unknown system";
	int ret;

	if(hi_linux_sysname_found)
		return hi_linux_sys_name;

	/* FIXME: DMI is x86-specific, should implement other platforms */

	ret = hi_linux_sysfs_readstr(SYS_DMI_PATH, "sys_vendor", NULL, vendor, 64);
	if(ret == HI_LINUX_SYSFS_OK) {
		hi_linux_sysfs_fixstr(vendor);
	}

	hi_linux_sysfs_readstr(SYS_DMI_PATH, "product_name", NULL, model, 128);
	if(ret == HI_LINUX_SYSFS_OK) {
		hi_linux_sysfs_fixstr(model);
	}

	snprintf(hi_linux_sys_name, SYSNAMELEN, "%s %s", vendor, model);
	hi_linux_sysname_found = B_TRUE;

	return hi_linux_sys_name;
}

/*Parses lsb_release output*/
static int read_lsb_release(void) {
	FILE* pipe = popen(LSB_RELEASE_PATH " -d -s 2>/dev/null", "r");
	char descr_str[128] = "";
	char *p_descr = descr_str, *end;
	size_t len;
	int tail;

	/* pipe() or fork() failed, failure! */
	if(pipe == NULL) {
		return OSNAME_ERROR;
	}

	fgets(descr_str, 128, pipe);
	len = strlen(descr_str);

	pclose(pipe);

	/*Not read release name, exit*/
	if(len == 0) {
		return OSNAME_ERROR;
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

	/* Cut out architecture label, or at least CR */
	end = strchr(descr_str, '(');
	if(!end || end == descr_str) {
		end = descr_str + len;
	}
	*--end = '\0';


	strncpy(hi_linux_os_name, p_descr, OSNAMELEN);

	return OSNAME_OK;
}

/* Read RedHat release
 *
 * It is contained in file /etc/redhat-release.
 * Cuts out word "release" and optional info in */
static int read_redhat_release(void) {
	FILE* releasef = NULL;
	char firstline[120];
	size_t len;
	int i, j;

	if(access("/etc/oracle-release", R_OK) == 0)
		releasef = fopen("/etc/oracle-release", "r");
	else
		releasef = fopen("/etc/redhat-release", "r");

	if(!releasef)
		return OSNAME_ERROR;

	fgets(firstline, 120, releasef);
	len = strlen(firstline);

	fclose(releasef);

	if(len == 0)
		return OSNAME_ERROR;

	for(i = 0, j = 0; i < len - 2 && j < 64; ++i, ++j) {
		/* Cut out word "release" */
		if(firstline[i] == 'r' &&
		   (strncmp(firstline + i, "release ", 8) == 0)) {
				i += 8;
		}

		if(firstline[i] == '(')
			break;

		hi_linux_os_name[j] = firstline[i];
	}

	hi_linux_os_name[j] = '\0';

	return OSNAME_OK;
}

static int read_suse_release(void) {
	FILE* releasef = fopen("/etc/SuSE-release", "r");
	char firstline[120];
	size_t len;
	char* end;

	if(!releasef)
		return OSNAME_ERROR;

	fgets(firstline, 120, releasef);
	len = strlen(firstline);

	fclose(releasef);

	if(len == 0)
		return OSNAME_ERROR;

	/* Cut out architecture label, or at least CR */
	end = strchr(firstline, '(');
	if(!end || end == firstline) {
		end = firstline + len;
	}
	*--end = '\0';

	strncpy(hi_linux_os_name, firstline, 64);

	return OSNAME_OK;
}
