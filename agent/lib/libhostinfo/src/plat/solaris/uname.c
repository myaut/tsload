/*
 * uname.c
 *
 *  Created on: 13.07.2013
 *      Author: myaut
 */

#include <uname.h>
#include <defs.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <sys/systeminfo.h>
#include <sys/utsname.h>

/* Based on /etc/release
 *
 * First line of release file looks like:
 * <lot of spaces> <name of Solaris> <Version fields> <KernelID> [SPARC|X86] <misc. info>
 *
 * Uses SPARC|X86 as an anchor to provide Kernel ID as OS release and rest of /etc/release's
 * first line as osname. Misc info is ignored
 *  */

/* bootadm uses 80 as a constant */
#define RELEASENAMELEN 		80

boolean_t hi_sol_release = B_FALSE;

PLATAPIDECL(hi_get_os_name, hi_get_os_release) char hi_sol_os_name[RELEASENAMELEN];
PLATAPIDECL(hi_get_os_name, hi_get_os_release) char hi_sol_os_release[RELEASENAMELEN];

char hi_sol_domain_name[64];

static void hi_sol_read_release(void);
static void hi_sol_uname(void);

PLATAPI const char* hi_get_os_name() {
	if(!hi_sol_release)
		hi_sol_read_release();

	return hi_sol_os_name;
}

PLATAPI const char* hi_get_os_release() {
	if(!hi_sol_release)
		hi_sol_read_release();

	return hi_sol_os_release;
}

PLATAPI const char* hi_get_domainname() {
	if(sysinfo(SI_SRPC_DOMAIN, hi_sol_domain_name, 64) >= 0)
		return hi_sol_domain_name;

	return "";
}


/* Reads release from /etc/release file
 * If fails, falls back to uname information */
static void hi_sol_read_release(void) {
	char firstline[RELEASENAMELEN];
	char *release, *osname, *end;

	FILE* releasef = fopen("/etc/release", "r");

	if(!releasef)
		return hi_sol_uname();

	if(fgets(firstline, RELEASENAMELEN, releasef) < 3) {
		fclose(releasef);
		return hi_sol_uname();
	}

	fclose(releasef);

	/* Search for architecture marking. It doen't reveal true machine architecture
	 * (32 or 64 bit), so we do not override uname() based implementation */

	end = strstr(firstline, "X86");
	if(!end) {
		end = strstr(firstline, "SPARC");
		if(!end) {
			return hi_sol_uname();
		}
	}
	/* Ignore preceding space */
	*(--end) = '\0';

	osname = firstline;
	while(isspace(*osname)) {
		++osname;
		if(osname == end)
			return hi_sol_uname();
	}

	release = end;
	while(!isspace(*release)) {
		--release;
		if(release == osname)
			release = "";
	}
	*(release++) = '\0';

	/*OK*/
	strncpy(hi_sol_os_name, osname, RELEASENAMELEN);
	strncpy(hi_sol_os_release, release, RELEASENAMELEN);

	hi_sol_release = B_TRUE;
}

static void hi_sol_uname(void) {
	struct utsname uname_data;

	uname(&uname_data);

	strcpy(hi_sol_os_name, "Unknown ");
	strcat(hi_sol_os_name, uname_data.sysname);

	strcpy(hi_sol_os_name, uname_data.release);

	hi_sol_release = B_TRUE;
}
