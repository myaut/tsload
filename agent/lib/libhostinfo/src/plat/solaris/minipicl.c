
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



#include <mempool.h>

#include <plat/minipicl.h>
#include <plat/mpiclimpl.h>

#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <door.h>
#include <sys/mman.h>

/*
 * Ouch! 64-bit version of Solaris libpicl is broken because it uses enum's in binary protocol:
 * (see more here: https://community.oracle.com/thread/3532313)
 *
 * Here is implementation of subset of libpicl for libhostinfo
 * */

static int picl_door = -1;

static int minipicl_call(picl_callnumber_t cnum, void* args, size_t argsize,
		             void* retbuf, size_t retsize) {
	door_arg_t darg;
	int ret = PICL_SUCCESS;

	size_t data_size = sizeof(picl_callnumber_impl_t) + argsize;
	picl_msg_t* data;
	size_t rsize = sizeof(picl_callnumber_impl_t) + retsize;
	picl_msg_t* rbuf;
	picl_msg_t* door_rbuf;
	picl_reterror_msg* error;

	if(picl_door < 0) {
		return PICL_NOTINITIALIZED;
	}

	data = mp_malloc(data_size);
	rbuf = mp_malloc(rsize);

	data->cnum = (picl_callnumber_impl_t) cnum;
	if(args) {
		memcpy(MPICL_MSG_PAYLOAD(data), args, argsize);
	}

	darg.data_ptr = data;
	darg.data_size = data_size;
	darg.desc_ptr = NULL;
	darg.desc_num = 0;
	darg.rbuf = rbuf;
	darg.rsize = rsize;

	if(door_call(picl_door, &darg) < 0) {
		ret = PICL_NORESPONSE;
		goto end;
	}

	door_rbuf = (picl_msg_t*) darg.rbuf;

	if(door_rbuf->cnum == cnum) {
		if(retbuf) {
			memcpy(retbuf, MPICL_MSG_PAYLOAD(door_rbuf), retsize);
		}
		goto end;
	}
	else if(door_rbuf->cnum == PICL_CNUM_ERROR) {
		error = MPICL_MSG_PAYLOAD(door_rbuf);
		if(error->in_cnum == cnum) {
			ret = error->error;
			goto end;
		}
	}

	/* Shouldn't be here */
	ret = PICL_UNKNOWNSERVICE;

end:
	if(door_rbuf != rbuf)
		munmap(door_rbuf, darg.rsize);

	mp_free(data);
	mp_free(rbuf);

	return ret;
}

int picl_get_root(picl_nodehdl_t *rooth) {
	return  minipicl_call(PICL_CNUM_GETROOT,
						  NULL, 0,
						  rooth, sizeof(picl_nodehdl_t));
}

/* ------------------------
 * picl_get_prop_by_name()
 * ------------------------ */

typedef struct {
	picl_nodehdl_t 		nodeh;
	char 				propname[PICL_PROPNAMELEN_MAX];
} PACKED_STRUCT minipicl_attrbyname_req_t;

typedef struct {
	minipicl_attrbyname_req_t	req;
	picl_prophdl_t	proph;
} PACKED_STRUCT minipicl_attrbyname_ret_t ;

int picl_get_prop_by_name(picl_nodehdl_t nodeh, const char *name,
			picl_prophdl_t *proph) {
	minipicl_attrbyname_req_t req;
	minipicl_attrbyname_ret_t ret;
	int err;

	req.nodeh = nodeh;
	strncpy(req.propname, name, PICL_PROPNAMELEN_MAX);

	err = minipicl_call(PICL_CNUM_GETATTRBYNAME,
			  	  	  	&req, sizeof(minipicl_attrbyname_req_t),
			  	  	  	&ret, sizeof(minipicl_attrbyname_ret_t));

	if(err != PICL_SUCCESS)
		return err;

	*proph = ret.proph;

	return PICL_SUCCESS;
}

/* ------------------------
 * picl_get_propinfo()
 * ------------------------ */

typedef struct {
	picl_prophdl_t		proph;
	int32_t				type;
	int32_t				accessmode;
	uint32_t			size;
	char 				propname[PICL_PROPNAMELEN_MAX];
} PACKED_STRUCT minipicl_attinfo_ret_t;

int picl_get_propinfo(picl_prophdl_t proph, picl_propinfo_t *pinfo) {
	int err;
	minipicl_attinfo_ret_t ret;

	err = minipicl_call(PICL_CNUM_GETATTRINFO,
				  	  	&proph, sizeof(picl_prophdl_t),
				  	  	&ret, sizeof(minipicl_attinfo_ret_t));

	if(err != PICL_SUCCESS)
		return err;

	pinfo->type = ret.type;
	pinfo->size = ret.size;
	pinfo->accessmode = ret.accessmode;

	return PICL_SUCCESS;
}

/* ------------------------
 * picl_get_propinfo_by_name()
 * ------------------------ */

int picl_get_propinfo_by_name(picl_nodehdl_t nodeh, const char *prop_name,
			picl_propinfo_t *pinfo, picl_prophdl_t *proph) {
	int err;

	err = picl_get_prop_by_name(nodeh, prop_name, proph);

	if(err != PICL_SUCCESS)
		return err;

	return picl_get_propinfo(*proph, pinfo);
}

/* ------------------------
 * picl_get_propval()
 * ------------------------ */

typedef struct {
	picl_prophdl_t	proph;
	uint32_t 		bufsize;
} PACKED_STRUCT minipicl_propval_req_t;

typedef struct {
	picl_prophdl_t	proph;
	uint32_t 		nbytes;
	char			val[0];
} PACKED_STRUCT minipicl_propval_ret_t;

int picl_get_propval(picl_prophdl_t proph, void *valbuf, size_t nbytes) {
	int err;

	minipicl_propval_req_t req;
	minipicl_propval_ret_t* p_ret;
	size_t retsize = sizeof(minipicl_propval_ret_t) + nbytes;

	req.proph = proph;
	req.bufsize = nbytes;

	p_ret = mp_malloc(retsize);

	err = minipicl_call(PICL_CNUM_GETATTRVAL,
						&req, sizeof(minipicl_propval_req_t),
						p_ret, retsize);

	if(err != PICL_SUCCESS) {
		goto end;
	}

	if(p_ret->nbytes > nbytes) {
		err = PICL_VALUETOOBIG;
	}
	else {
		memcpy(valbuf, &p_ret->val, p_ret->nbytes);
	}

end:
	mp_free(p_ret);

	return err;
}

/* ------------------------
 * picl_get_propval_by_name()
 * ------------------------ */

typedef struct {
	picl_nodehdl_t  nodeh;
	char 			propname[PICL_PROPNAMELEN_MAX];
	uint32_t 		bufsize;
} PACKED_STRUCT minipicl_propvalname_req_t;

typedef struct {
	picl_nodehdl_t  nodeh;
	char 			propname[PICL_PROPNAMELEN_MAX];
	uint32_t 		nbytes;
	char			val[0];
} PACKED_STRUCT minipicl_propvalname_ret_t;

int picl_get_propval_by_name(picl_nodehdl_t nodeh, const char *propname,
			void *valbuf, size_t nbytes) {
	int err;

	minipicl_propvalname_req_t req;
	minipicl_propvalname_ret_t* p_ret;
	size_t retsize = sizeof(minipicl_propvalname_ret_t) + nbytes;

	req.nodeh = nodeh;
	strncpy(req.propname, propname, PICL_PROPNAMELEN_MAX);
	req.bufsize = nbytes;

	p_ret = mp_malloc(retsize);

	err = minipicl_call(PICL_CNUM_GETATTRVALBYNAME,
						&req, sizeof(minipicl_propvalname_req_t),
						p_ret, retsize);

	if(err != PICL_SUCCESS) {
		goto end;
	}

	if(p_ret->nbytes > nbytes) {
		err = PICL_VALUETOOBIG;
	}
	else {
		memcpy(valbuf, &p_ret->val, p_ret->nbytes);
	}

end:
	mp_free(p_ret);

	return err;
}


/* ------------------------
 * picl_walk_tree_by_class()
 * NOTE: Implementation is taken from original libpicl.
 * ------------------------ */

static int minipicl_walk(picl_nodehdl_t rooth, const char *classname,
		void *c_args, int (*callback_fn)(picl_nodehdl_t hdl, void *args)) {
	int		err;
	picl_nodehdl_t	chdh;
	char	classval[PICL_CLASSNAMELEN_MAX];

	err = picl_get_propval_by_name(rooth, PICL_PROP_CHILD, &chdh, sizeof (chdh));

	while (err == PICL_SUCCESS) {
		err = picl_get_propval_by_name(chdh, PICL_PROP_CLASSNAME, classval, sizeof (classval));

		if (err != PICL_SUCCESS)
			return err;

		if ((classname == NULL) || (strcmp(classname, classval) == 0)) {
			err = callback_fn(chdh, c_args);
			if (err != PICL_WALK_CONTINUE)
				return err;
		}

		err = minipicl_walk(chdh, classname, c_args, callback_fn);
		if (err != PICL_WALK_CONTINUE)
			return err;

		err = picl_get_propval_by_name(chdh, PICL_PROP_PEER, &chdh, sizeof (chdh));
	}

	if (err == PICL_PROPNOTFOUND)	/* end of a branch */
		return PICL_WALK_CONTINUE;

	return err;

}

int picl_walk_tree_by_class(picl_nodehdl_t rooth, const char *classname,
		void *c_args, int (*callback_fn)(picl_nodehdl_t hdl, void *args)) {
	int		err;

	if (callback_fn == NULL)
		return PICL_INVALIDARG;

	err = minipicl_walk(rooth, classname, c_args, callback_fn);

	if (err == PICL_WALK_CONTINUE || err == PICL_WALK_TERMINATE)
		return PICL_SUCCESS;

	return err;
}

/* ------------------------
 * picl_initialize()
 * ------------------------ */

int picl_initialize(void) {
	int err;
	uint32_t clrev = PICLD_DOOR_VERSION;
	int32_t srvrev;

	picl_door = open(PICLD_DOOR_11, O_RDONLY);

	if(picl_door < 0) {
		picl_door = open(PICLD_DOOR_10, O_RDONLY);

		if(picl_door < 0) {
			return PICL_NOTINITIALIZED;
		}
	}

	err = minipicl_call(PICL_CNUM_INIT,
						&clrev, sizeof(uint32_t),
						&srvrev, sizeof(int32_t));

	if(err != PICL_SUCCESS) {
		close(picl_door);
		picl_door = -1;

		return err;
	}

	return PICL_SUCCESS;
}

/* ------------------------
 * picl_shutdown()
 * ------------------------ */

int picl_shutdown(void) {
	int err;

	err = minipicl_call(PICL_CNUM_FINI,
						NULL, 0, NULL, 0);

	if(err == PICL_SUCCESS) {
		close(picl_door);
		picl_door = -1;
	}

	return err;
}

