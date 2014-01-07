/*
 * tsfutil.h
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#ifndef TSFUTIL_H_
#define TSFUTIL_H_

#define FORMAT_JSON			0
#define FORMAT_CSV			1

#define COMMAND_CREATE		0
#define COMMAND_ADD			1
#define COMMAND_GET_COUNT	2
#define COMMAND_GET_ENTRIES 3

tsfile_schema_t* schema_read(const char* filename);

#endif /* TSFUTIL_H_ */
