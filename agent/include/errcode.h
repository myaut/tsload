
/*
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
*/  



#ifndef ERRCODE_H_
#define ERRCODE_H_

typedef enum ts_errcode {
	TSE_OK					 = 0,

	TSE_COMMAND_NOT_FOUND 	 = 100,

	TSE_MESSAGE_FORMAT		 = 200,
	TSE_MISSING_ATTRIBUTE	 = 201,
	TSE_UNUSED_ATTRIBUTE	 = 202,
	TSE_INVALID_TYPE		 = 203,
	TSE_INVALID_VALUE		 = 204,
	TSE_NOT_FOUND			 = 205,
	TSE_ALREADY_EXISTS		 = 206,

	TSE_INTERNAL_ERROR		 = 300,
	TSE_INVALID_STATE		 = 301
} ts_errcode_t;

/* TODO: deprecated */
#define TSE_INVALID_DATA		TSE_INVALID_VALUE

#endif /* ERRCODE_H_ */

