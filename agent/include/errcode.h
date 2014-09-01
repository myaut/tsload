/*
 * errcode.h
 *
 *  Created on: 08.01.2013
 *      Author: myaut
 */

#ifndef ERRCODE_H_
#define ERRCODE_H_

typedef enum ts_errcode {
	TSE_OK					 = 0,

	TSE_COMMAND_NOT_FOUND 	 = 100,

	TSE_MESSAGE_FORMAT		 = 200,
	TSE_MISSING_ATTRIBUTE	 = 201,
	TSE_INVALID_TYPE		 = 202,
	TSE_INVALID_VALUE		 = 203,

	TSE_INTERNAL_ERROR		 = 300,
	TSE_INVALID_STATE		 = 301
} ts_errcode_t;

/* XXX: deprecated */
#define TSE_INVALID_DATA		TSE_INVALID_VALUE

#endif /* ERRCODE_H_ */
