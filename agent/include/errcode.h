/*
 * errcode.h
 *
 *  Created on: 08.01.2013
 *      Author: myaut
 */

#ifndef ERRCODE_H_
#define ERRCODE_H_

typedef enum ts_errcode {
	TSE_COMMAND_NOT_FOUND 	 = 100,
	TSE_MESSAGE_FORMAT		 = 101,
	TSE_INVALID_DATA		 = 102,
	TSE_INVALID_STATE		 = 103,
	TSE_INTERNAL_ERROR		 = 200
} ts_errcode_t;

#endif /* ERRCODE_H_ */
