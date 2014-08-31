
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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

#ifndef TSOBJ_H_
#define TSOBJ_H_

#include <defs.h>

#include <json.h>

typedef void tsobj_node_t;
typedef json_type_t tsobj_type_t;
typedef json_str_t tsobj_str_t;

#define TSOBJ_GEN
#include <tsobjgen.h>
#undef TSOBJ_GEN

/**
 * Iterate over children of TSOBJ node
 *
 * @param parent	parent node
 * @param node		iterator
 * @param id		int variable that contains index of child
 */
#define tsobj_for_each(parent, node, id) 			\
			for(node = tsobj_first(parent, &id);		\
				!tsobj_is_end(parent, node, &id);	\
				node = tsobj_next(node, &id))

#endif
