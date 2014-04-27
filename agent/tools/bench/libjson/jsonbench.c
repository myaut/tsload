/*
 * jsonbench.c
 *
 *  Created on: Apr 27, 2014
 *      Author: myaut
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void libjson_parse_bench(char* data, int iterations);
void tsjson_parse_bench(char* data, int iterations, size_t sz);

int main(int argc, char* argv[]) {
	char* mode;
	FILE* file;
	char* data;
	size_t length;

	if(argc < 3) {
		fprintf(stderr, "Usage: jsonbench {libjson|tsjson} file.json\n");
		return 1;
	}

	mode = argv[1];
	file = fopen(argv[2], "r");

	if(file == NULL) {
		fprintf(stderr, "Couldn't open file!\n");
		return 1;
	}

	/* Get length of file than rewind */
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* Read data from file */
	data = malloc(length + 1);
	fread(data, length, 1, file);
	data[length] = '\0';

	if(strcmp(mode, "libjson") == 0) {
		libjson_parse_bench(data, 10000);
	}
	else if(strcmp(mode, "tsjson") == 0) {
		tsjson_parse_bench(data, 10000, length);
	}

	free(data);

	return 0;
}
