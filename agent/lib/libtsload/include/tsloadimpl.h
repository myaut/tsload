/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, Tune-IT

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

#ifndef TSLOADIMPL_H_
#define TSLOADIMPL_H_

#include <tsload/modules.h>
#include <tsload/obj/obj.h>

struct tsload_param;

tsobj_node_t* tsobj_params_format_helper(struct tsload_param* params);
void tsobj_module_format_helper(tsobj_node_t* parent, module_t* mod);

#endif