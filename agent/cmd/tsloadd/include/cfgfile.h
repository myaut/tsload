/*
 * config.h
 *
 *  Created on: Nov 19, 2012
 *      Author: myaut
 */

#ifndef CFGFILE_H_
#define CFGFILE_H_

#define CONFPATHLEN		512
#define CONFLINELEN		1024

#define CFG_OK	0
#define CFG_ERR_NOFILE	-1
#define CFG_ERR_MISSING_EQ		-2
#define CFG_ERR_INVALID_OPTION	-3

int cfg_init(const char* cfg_file_name);

#endif /* CFGFILE_H_ */
