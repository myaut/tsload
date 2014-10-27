
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



#ifndef SYSFS_H_
#define SYSFS_H_

#include <defs.h>

#define HI_LINUX_SYSFS_BLOCK	32

#define HI_LINUX_SYSFS_OK		0
#define HI_LINUX_SYSFS_ERROR 	-1

int hi_linux_sysfs_readstr(const char* root, const char* name, const char* object,
						   char* str, int len);
int hi_linux_sysfs_readstr_aas(const char* root, const char* name, const char* object,
						   	   char** aas);
uint64_t hi_linux_sysfs_readuint(const char* root, const char* name, const char* object,
						   uint64_t defval);
int hi_linux_sysfs_readbitmap(const char* root, const char* name, const char* object,
						   uint32_t* bitmap, int len);
void hi_linux_sysfs_fixstr(char* p);
void hi_linux_sysfs_fixstr2(char* p);

int hi_linux_sysfs_walk(const char* root,
					   void (*proc)(const char* name, void* arg), void* arg);
int hi_linux_sysfs_walkbitmap(const char* root, const char* name, const char* object, int count,
					   	      void (*proc)(int id, void* arg), void* arg);

#include <hitrace.h>

#endif /* SYSFS_H_ */

