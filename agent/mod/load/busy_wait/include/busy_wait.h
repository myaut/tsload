
/*
    This file is part of TSLoad.
    Copyright 2012-2014, Sergey Klyaus, ITMO University

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



#ifndef BUSY_WAIT_H_
#define BUSY_WAIT_H_

#include <tsload/defs.h>

#include <tsload/load/wlparam.h>


struct busy_wait_workload {
	int unused;
};

struct busy_wait_request {
	wlp_integer_t num_cycles;
};


#endif /* BUSY_WAIT_H_ */

