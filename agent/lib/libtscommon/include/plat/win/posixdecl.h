/*
 * posixdecl.h
 *
 *  Created on: 23.12.2012
 *      Author: myaut
 */

#ifndef PLAT_WIN_POSIXDECL_H_
#define PLAT_WIN_POSIXDECL_H_

#include <defs.h>

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <stdlib.h>

/* POSIX-compliant functions have underline prefixes in Windows,
 * so make aliases for them*/
#define open	_open
#define close	_close
#define lseek 	_lseek
#define read	_read
#define write	_write
#define access	_access
#define mkdir   _mkdir
#define fileno	_fileno

/* See http://msdn.microsoft.com/en-us/library/1w06ktdy%28v=vs.71%29.aspx */
#define 	F_OK	00
#define		R_OK	02
#define 	W_OK	04

#define O_CREAT		_O_CREAT
#define O_SYNC		0


#if !defined(S_IREAD) && !defined(S_IWRITE)  && !defined(S_IEXEC)
#define S_IREAD		_S_IREAD
#define S_IWRITE	_S_IWRITE
#define S_IEXEC		_S_IEXEC
#endif

#define S_IRUSR		 S_IREAD
#define S_IWUSR		 S_IWRITE
#define S_IRWXU		( S_IWRITE | S_IWUSR | S_IEXEC )

#define S_IRGRP		0
#define S_IWGRP		0
#define S_IXGRP		0
#define S_IRWXG		0

#define S_IROTH		0
#define S_IWOTH		0
#define S_IXOTH		0
#define S_IRWXO		0

#define S_ISUID		0
#define S_ISGID 	0

#endif /* PLAT_WIN_POSIXDECL_H_ */
