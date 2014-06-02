
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



#ifndef MINIPICL_H_
#define MINIPICL_H_

#include <defs.h>

#define	PICL_PROPNAMELEN_MAX	256
#define	PICL_CLASSNAMELEN_MAX	(PICL_PROPNAMELEN_MAX - sizeof ("__"))

#define PICL_SUCCESS			0
#define PICL_FAILURE    		1
#define PICL_NORESPONSE 		2
#define PICL_UNKNOWNSERVICE     3
#define PICL_NOTINITIALIZED     4
#define PICL_INVALIDARG 		5
#define PICL_VALUETOOBIG        6
#define PICL_PROPNOTFOUND       7
#define PICL_NOTTABLE   		8
#define PICL_NOTNODE    		9
#define PICL_NOTPROP    		10
#define PICL_ENDOFLIST  		11
#define PICL_PROPEXISTS 		12
#define PICL_NOTWRITABLE        13
#define PICL_PERMDENIED 		14
#define PICL_INVALIDHANDLE      15
#define PICL_STALEHANDLE        16
#define PICL_NOTSUPPORTED       17
#define PICL_TIMEDOUT   		18
#define PICL_CANTDESTROY        19
#define PICL_TREEBUSY   		20
#define PICL_CANTPARENT 		21
#define PICL_RESERVEDNAME       22
#define PICL_INVREFERENCE       23
#define PICL_WALK_CONTINUE      24
#define PICL_WALK_TERMINATE     25
#define PICL_NODENOTFOUND       26
#define PICL_NOSPACE   			27
#define PICL_NOTREADABLE        28
#define PICL_PROPVALUNAVAILABLE	29

typedef uint64_t picl_nodehdl_t;
typedef uint64_t picl_prophdl_t;

typedef	enum {
	PICL_PTYPE_UNKNOWN = 0x0,
	PICL_PTYPE_VOID,
	PICL_PTYPE_INT,
	PICL_PTYPE_UNSIGNED_INT,
	PICL_PTYPE_FLOAT,
	PICL_PTYPE_REFERENCE,
	PICL_PTYPE_TABLE,
	PICL_PTYPE_TIMESTAMP,
	PICL_PTYPE_BYTEARRAY,
	PICL_PTYPE_CHARSTRING
} picl_prop_type_t;

typedef struct {
	picl_prop_type_t	type;
	unsigned int		accessmode;
	size_t				size;
	char				name[PICL_PROPNAMELEN_MAX];
} picl_propinfo_t;

#define	PICL_NODE_ROOT		"/"
#define	PICL_NODE_PLATFORM	"platform"
#define	PICL_NODE_OBP		"obp"
#define	PICL_NODE_FRUTREE	"frutree"

#define	PICL_PROP_NAME		"name"
#define	PICL_PROP_CLASSNAME	"_class"
#define	PICL_PROP_PARENT	"_parent"
#define	PICL_PROP_CHILD		"_child"
#define	PICL_PROP_PEER		"_peer"

#define	PICL_CLASS_PICL		"picl"

int picl_get_root(picl_nodehdl_t *rooth);

int picl_get_propval(picl_prophdl_t proph, void *valbuf, size_t nbytes);
int picl_get_propval_by_name(picl_nodehdl_t nodeh, const char *propname,
			void *valbuf, size_t nbytes);

int picl_get_prop_by_name(picl_nodehdl_t nodeh, const char *name,
    picl_prophdl_t *proph);

int picl_get_propinfo(picl_prophdl_t proph, picl_propinfo_t *pinfo);
int picl_get_propinfo_by_name(picl_nodehdl_t nodeh, const char *prop_name,
				picl_propinfo_t *pinfo, picl_prophdl_t *proph);

int picl_walk_tree_by_class(picl_nodehdl_t rooth, const char *classname,
		void *c_args, int (*callback_fn)(picl_nodehdl_t hdl, void *args));

int picl_initialize(void);
int picl_shutdown(void);

#endif /* MINIPICL_H_ */

