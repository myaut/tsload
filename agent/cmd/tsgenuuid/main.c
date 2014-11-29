/*
 * main.c
 *
 * Main file for ts-loader daemon
 */


#include <tsload/defs.h>

#include <tsload/mempool.h>
#include <tsload/getopt.h>
#include <tsload/version.h>
#include <tsload/pathutil.h>

#include <genuuid.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <plat/posixdecl.h>


#define BUFLEN		256

char loadcfg_path[PATHMAXLEN];
char moncfg_path[PATHMAXLEN];

void usage(int err, const char* reason, ...);

void parse_options(int argc, char* argv[]) {
	int ok = 1;

	int c;

	boolean_t lflag = B_FALSE;
	boolean_t mflag = B_FALSE;

	while((c = plat_getopt(argc, argv, "hvl:m:")) != -1) {
		switch(c) {
		case 'v':
			print_ts_version("Agent UUID generator");
			exit(0);
			break;
		case 'l':
			strncpy(loadcfg_path, optarg, PATHMAXLEN);
			lflag = B_TRUE;
			break;
		case 'm':
			strncpy(moncfg_path, optarg, PATHMAXLEN);
			mflag = B_TRUE;
			break;
		case 'h':
			usage(0, "");
			break;
		case '?':
			usage(1, "Unknown option `-%c'.\n", optopt);
			break;
		}
	}

	if(!lflag /* || !mflag */) {
		usage(1, "Config paths are not specified");
	}
}

void edit_cfg_file(const char* path, const char* uuid) {
	char old_path[PATHMAXLEN];
	char buf[BUFLEN], buf2[BUFLEN];
	char* s;

	size_t len;

	FILE *old, *new;

	boolean_t printed = B_FALSE;

	const char* fmt = "AGENTUUID=%s\n";

	strcpy(old_path, path);
	strncat(old_path, ".old", PATHMAXLEN);

	if(rename(path, old_path) == -1) {
		perror(path);
		exit(1);
	}

	old = fopen(old_path, "r");
	new = fopen(path, "w");

	if(!old || !new) {
		fprintf(stderr, "Failed to open config file %s", path);
		exit(1);
	}

	/* Process config files */
	while((s = fgets(buf, BUFLEN, old)) != NULL) {
		len = strlen(s);

		if(len > 10 && strncmp(s, "AGENTUUID", 9) == 0) {
			/* AGENTUUID param - save old and write new */
			snprintf(buf2, BUFLEN, "# %s", s);
			fputs(buf2, new);

			snprintf(buf2, BUFLEN, fmt, uuid);
			fputs(buf2, new);

			printed = B_TRUE;
		}
		else {
			/* Put line as is */
			fputs(s, new);
		}
	}

	if(!printed) {
		fputs("# Generated automatically by tsgenuuid\n", new);

		snprintf(buf2, BUFLEN, fmt, uuid);
		fputs(buf2, new);

		fputs("# End of agent UUIDs\n", new);
	}

	fclose(old);
	fclose(new);
}

void gen_uuid(void) {
	uuid_str_t uu;

	if(generate_uuid(uu) != UUID_OK) {
		fprintf(stderr, "Failed to generate UUID\n");
		exit(1);
	}

	printf("UUID: %s\n", uu);

	edit_cfg_file(loadcfg_path, uu);
}

int main(int argc, char* argv[]) {
	int err = 0;
	int i;

	parse_options(argc, argv);

	/* Check if load config accessible */
	if(access(loadcfg_path, W_OK) != 0) {
		fprintf(stderr, "Loader config file '%s' is not accessible or not exists.\n", loadcfg_path);
		exit(1);
	}

	gen_uuid();

	return 0;
}
