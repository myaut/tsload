/*
 * sysfs.h
 *
 *  Created on: 02.05.2013
 *      Author: myaut
 */

#ifndef SYSFS_H_
#define SYSFS_H_

#include <defs.h>

#define HI_LINUX_SYSFS_OK		0
#define HI_LINUX_SYSFS_ERROR 	-1

int hi_linux_sysfs_readstr(const char* root, const char* name, const char* object,
						   char* str, int len);
uint64_t hi_linux_sysfs_readuint(const char* root, const char* name, const char* object,
						   uint64_t defval);
int hi_linux_sysfs_readbitmap(const char* root, const char* name, const char* object,
						   uint32_t* bitmap, int len);
void hi_linux_sysfs_fixstr(char* p);

int hi_linux_sysfs_walk(const char* root,
					   void (*proc)(const char* name, void* arg), void* arg);
int hi_linux_sysfs_walkbitmap(const char* root, const char* name, const char* object, int count,
					   	      void (*proc)(int id, void* arg), void* arg);

#include <hitrace.h>

#endif /* SYSFS_H_ */
