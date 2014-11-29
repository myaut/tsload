/*
 * Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef MINIIPADM_H_
#define MINIIPADM_H_

#include <tsload/defs.h>

#include <libdladm.h>
#include <sys/types.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>


#define	IPADM_AOBJ_USTRSIZ	32
#define	IPADM_AOBJSIZ		(LIFNAMSIZ + IPADM_AOBJ_USTRSIZ)

/* error codes */
typedef enum {
	IPADM_SUCCESS,
	IPADM_FAILURE

	/* Most of options are omitted */
} ipadm_status_t;

/* options */
#define	IPADM_OPT_PERSIST	0x00000001
#define	IPADM_OPT_ACTIVE	0x00000002
#define	IPADM_OPT_DEFAULT	0x00000004
#define	IPADM_OPT_PERM		0x00000008

typedef struct ipadm_handle {
	int		ih_sock;
	int		ih_sock6;
	int		ih_door_fd;
	int		ih_rtsock;
	dladm_handle_t	ih_dh;
	uint32_t	ih_flags;
	pthread_mutex_t	ih_lock;
	zoneid_t	ih_zoneid;
} ipadm_handle_t;

/* ipadm_if_info_t states */
typedef enum {
	IPADM_IFS_DOWN,
	IPADM_IFS_OK,
	IPADM_IFS_FAILED,
	IPADM_IFS_OFFLINE,
	IPADM_IFS_DISABLED
} ipadm_if_state_t;

/* possible address types */
typedef enum  {
	IPADM_ADDR_NONE,
	IPADM_ADDR_STATIC,
	IPADM_ADDR_IPV6_ADDRCONF,
	IPADM_ADDR_DHCP
} ipadm_addr_type_t;

typedef struct ipadm_addr_info_s {
	struct ifaddrs		ia_ifa;		/* list of addresses */
	char			ia_sname[NI_MAXHOST];	/* local hostname */
	char			ia_dname[NI_MAXHOST];	/* remote hostname */
	char			ia_aobjname[IPADM_AOBJSIZ];
	uint_t			ia_cflags;	/* active flags */
	uint_t			ia_pflags;	/* persistent flags */
	int32_t			ia_atype;	/* see above */
	int32_t			ia_state;	/* see above */
	time_t			ia_lease_begin;
	time_t			ia_lease_expire;
	time_t			ia_lease_renew;
	int32_t			ia_clientid_type;
	char			*ia_clientid;
} ipadm_addr_info_t;

LIBEXPORT ipadm_status_t ipadm_open(ipadm_handle_t *, uint32_t);
LIBEXPORT void ipadm_close(ipadm_handle_t);

LIBEXPORT ipadm_status_t ipadm_addr_info(ipadm_handle_t, const char *,
			    ipadm_addr_info_t **, uint32_t, int64_t);
LIBEXPORT void ipadm_free_addr_info(ipadm_addr_info_t *);

#endif /* MINIIPADM_H_ */
