
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

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

