
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#ifndef MPICLIMPL_H_
#define MPICLIMPL_H_

#include <tsload/defs.h>


#define	PICLD_DOOR_VERSION	1
#define	PICLD_DOOR_11	"/system/volatile/picld_door"
#define	PICLD_DOOR_10	"/var/run/picld_door"
#define	PICLD_DOOR_COOKIE	((void *)(0xdeaffeed ^ PICLD_DOOR_VERSION))

typedef enum {
	PICL_CNUM_INIT = 0x1,
	PICL_CNUM_FINI,
	PICL_CNUM_GETROOT,
	PICL_CNUM_GETATTRVAL,
	PICL_CNUM_GETATTRVALBYNAME,
	PICL_CNUM_GETATTRINFO,
	PICL_CNUM_GETFIRSTATTR,
	PICL_CNUM_GETNEXTATTR,
	PICL_CNUM_GETATTRBYNAME,
	PICL_CNUM_GETATTRBYROW,
	PICL_CNUM_GETATTRBYCOL,
	PICL_CNUM_SETATTRVAL,
	PICL_CNUM_SETATTRVALBYNAME,
	PICL_CNUM_PING,
	PICL_CNUM_WAIT,
	PICL_CNUM_ERROR,
	PICL_CNUM_FINDNODE,
	PICL_CNUM_NODEBYPATH,
	PICL_CNUM_FRUTREEPARENT
} picl_callnumber_t;

typedef int32_t picl_callnumber_impl_t;
typedef int32_t picl_errno_t;

#define MPICL_MSG_PAYLOAD(msg)		(&((picl_msg_t*) msg)->payload)

typedef struct {
	picl_callnumber_t cnum;
	int payload[0];
} PACKED_STRUCT picl_msg_t;

typedef struct {
	picl_callnumber_t in_cnum;
	picl_errno_t error;
} PACKED_STRUCT picl_reterror_msg;

#endif /* MPICLIMPL_H_ */

