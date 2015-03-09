/*
    This file is part of TSLoad.
    Copyright 2015, Sergey Klyaus, Tune-IT

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

#include <tsload/autostring.h>
#include <tsload/mempool.h>

#include <hostinfo/fsinfo.h>
#include <hostinfo/diskinfo.h>

#include <hitrace.h>

PLATAPI int hi_fsinfo_probe_impl(list_head_t* disks) {
	return HI_PROBE_OK;
}