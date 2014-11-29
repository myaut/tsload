
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



#ifndef GENUUID_H_
#define GENUUID_H_

#include <tsload/defs.h>


#define UUID_OK				0
#define UUID_ERROR			1

#define UUIDSTRMAXLEN		37

typedef char uuid_str_t[UUIDSTRMAXLEN];

PLATAPI int generate_uuid(uuid_str_t uu);

#endif

