/*
 * helpers.h
 *
 *  Created on: Sep 2, 2014
 *      Author: myaut
 */

#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdio.h>

#include <assert.h>

#define JSON_PROP(name, value) "\"" name "\": " #value
#define JSON_PROP2(name, json) "\"" name "\": " json

STATIC_INLINE void debug_json_assert(int ret) {
	if(ret != JSON_OK) {
		fprintf(stderr, "JSON error: %s\n", json_error_message());
	}

	assert(ret == JSON_OK);
}

#define TEST_PREAMBLE(conf_str)						\
	static char conf[] = conf_str;					\
	tsobj_node_t* node = NULL;						\
	fprintf(stderr, "%s:\n%s\n", __func__, conf);	\
	debug_json_assert(								\
			json_parse(JSON_BUFFER(conf),			\
					   (json_node_t**) &node));

#define WLP_TEST_PREAMBLE(pname, str)			\
	json_node_t* param;							\
	TEST_PREAMBLE(str);							\
	param = json_find(node, pname);

#endif /* HELPERS_H_ */
