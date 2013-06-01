/*
 * posixdecl.h
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#ifndef PLAT_WIN_POSIXDECL_H_
#define PLAT_WIN_POSIXDECL_H_

#include <io.h>
#include <fcntl.h>
#include <stdlib.h>

/* POSIX-compliant functions have underline prefixes in Windows,
 * so make aliases for them*/
#define open	_open
#define close	_close
#define lseek 	_lseek
#define read	_read
#define write	_write
#define access	_access

/* See http://msdn.microsoft.com/en-us/library/1w06ktdy%28v=vs.71%29.aspx */
#define 	F_OK	00
#define		R_OK	02
#define 	W_OK	04

#endif /* PLAT_WIN_POSIXDECL_H_ */
