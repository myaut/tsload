/*
 * tsfile.h
 *
 *  Created on: Jan 7, 2014
 *      Author: myaut
 */

#ifndef TSFILE_H_
#define TSFILE_H_

#include <defs.h>
#include <errcode.h>
#include <tstime.h>
#include <atomic.h>
#include <threads.h>

#include <stdio.h>

#ifndef NO_JSON
#include <libjson.h>
#endif

#define TSFILE_EXTENSION	"tsf"

#define TSFILE_MAGIC_LEN	6
#define TSFILE_MAGIC		"TSFILE"
#define TSFILE_VERSION		1

#define SBCOUNT			4
#define SBMASK			(SBCOUNT - 1)

#define MAXFIELDLEN		32
#define MAXFIELDCOUNT	64

#define TSFILE_HEADER_SIZE	4096
#define	TSFILE_SB_WRITE_LEN	512

typedef struct tsfile_sb {
	ts_time_t 	time;
	uint32_t	count;
} tsfile_sb_t;

typedef enum tsfile_ftype {
	TSFILE_FIELD_BOOLEAN,
	TSFILE_FIELD_INT,
	TSFILE_FIELD_FLOAT,
	TSFILE_FIELD_STRING,

	TSFILE_FIELD_TYPE_MAX
} tsfile_ftype_t;

typedef struct tsfile_field {
	char		   name[MAXFIELDLEN];
	tsfile_ftype_t type;
	size_t		   size;
	ptrdiff_t	   offset;
} tsfile_field_t;

typedef struct tsfile_shdr {
	uint16_t		entry_size;
	uint16_t		count;
	uint32_t		pad1;
	uint64_t		pad2;
} tsfile_shdr_t;

typedef struct tsfile_schema {
	tsfile_shdr_t   hdr;
	tsfile_field_t  fields[MAXFIELDCOUNT];
} tsfile_schema_t;

typedef struct tsfile_header {
	char			magic[TSFILE_MAGIC_LEN];
	uint16_t		version;

	tsfile_sb_t		sb[SBCOUNT];
	tsfile_schema_t schema;
} tsfile_header_t;

typedef struct tsfile {
	tsfile_header_t* header;

	int 			fd;
	char* 			filename;
	size_t			size;

	int				cur_sb;
	thread_mutex_t	mutex;

#ifndef NO_JSON
	JSONNODE**		node_cache;
#else
	void**			node_cache;
#endif
	int 			node_first;
	int 			node_count;
	thread_mutex_t	node_mutex;
} tsfile_t;

#define TSFILE_SCHEMA_HEADER(a_entry_size, a_field_count)		\
		{														\
			SM_INIT(.entry_size, a_entry_size),					\
			SM_INIT(.count, a_field_count)						\
		}

#define TSFILE_SCHEMA_FIELD(a_name, a_type, a_size, a_offset)	\
		{														\
			SM_INIT(.name, a_name),								\
			SM_INIT(.type, a_type),								\
			SM_INIT(.size, a_size),								\
			SM_INIT(.offset, a_offset)							\
		}

#define TSFILE_SCHEMA_FIELD_REF(struct_type, member, a_type)	\
		TSFILE_SCHEMA_FIELD( #member, a_type,					\
							 sizeof(((struct_type*)0)->member),	\
							 offsetof(struct_type, member))

typedef void (*tsfile_error_msg_func)(ts_errcode_t errcode, const char* format, ...);
LIBIMPORT tsfile_error_msg_func tsfile_error_msg;

#define		TSFILE_OK		   0
#define		TSFILE_SB_FAIL	   -1
#define 	TSFILE_DATA_FAIL   -2
#define 	TSFILE_INVAL_RANGE -3

LIBIMPORT	int tsfile_errno;

/* Schema API */

#define SCHEMA_FIELD_OK				0
#define SCHEMA_FIELD_MISSING_TYPE	1
#define SCHEMA_FIELD_INVALID_TYPE	2
#define SCHEMA_FIELD_MISSING_OFF	3
#define SCHEMA_FIELD_MISSING_SIZE	4

LIBEXPORT tsfile_schema_t* tsfile_schema_read(const char* filename);
LIBEXPORT int tsfile_schema_write(const char* filename, tsfile_schema_t* schema);

LIBEXPORT tsfile_schema_t* tsfile_schema_alloc(int field_count);
LIBEXPORT tsfile_schema_t* tsfile_schema_clone(int ext_field_count, tsfile_schema_t* base);

#ifndef NO_JSON
LIBEXPORT tsfile_schema_t* json_tsfile_schema_proc(JSONNODE* root, boolean_t auto_offset);
LIBEXPORT JSONNODE* json_tsfile_schema_format(tsfile_schema_t* schema);
#endif


/* Nodes API (internal) */

void tsfile_init_nodes(tsfile_t* file);
void tsfile_destroy_nodes(tsfile_t* file);

#ifndef NO_JSON
JSONNODE* tsfile_create_node(tsfile_t* file);
JSONNODE** tsfile_get_nodes(tsfile_t* file, int count);
boolean_t tsfile_put_node(tsfile_t* file, JSONNODE* node);

void tsfile_fill_node(tsfile_t* file, JSONNODE* node, void* entry);
int tsfile_fill_entry(tsfile_t* file, JSONNODE* node, void* entry);
#endif

/* TSFile API */

LIBEXPORT tsfile_t* tsfile_create(const char* filename, tsfile_schema_t* schema);
LIBEXPORT tsfile_t* tsfile_open(const char* filename, tsfile_schema_t* schema);
LIBEXPORT void tsfile_close(tsfile_t* file);

LIBEXPORT int tsfile_add(tsfile_t* file, void* entries, unsigned count);
LIBEXPORT uint32_t tsfile_get_count(tsfile_t* file);
LIBEXPORT int tsfile_get_entries(tsfile_t* file, void* entries, unsigned start, unsigned end);

#ifndef NO_JSON
LIBEXPORT JSONNODE* json_tsfile_get(tsfile_t* file, unsigned number);
LIBEXPORT void json_tsfile_put(tsfile_t* file, JSONNODE* node);
LIBEXPORT int json_tsfile_add(tsfile_t* file, JSONNODE* node);

LIBEXPORT JSONNODE* json_tsfile_get_array(tsfile_t* file, unsigned start, unsigned end);
LIBEXPORT void json_tsfile_put_array(tsfile_t* file, JSONNODE* node_array);
LIBEXPORT int json_tsfile_add_array(tsfile_t* file, JSONNODE* node_array);

#endif

LIBEXPORT int tsfile_init(void);
LIBEXPORT void tsfile_fini(void);

/* Backends */

struct tsf_backend_class;

typedef struct tsf_backend {
	FILE* file;
	tsfile_t* ts_file;

	struct tsf_backend_class* tsf_class;
	void* private;
} tsf_backend_t;

typedef int (*tsf_get_func)(tsf_backend_t* backend, int start, int end);
typedef int (*tsf_add_func)(tsf_backend_t* backend);

typedef int (*tsf_set_func)(tsf_backend_t* backend, const char* option);

typedef struct tsf_backend_class {
	tsf_set_func set;

	tsf_get_func get;
	tsf_add_func add;
} tsf_backend_class_t;

tsf_backend_t* tsfile_backend_create(const char* name);
void tsfile_backend_set_files(tsf_backend_t* backend, FILE* file, tsfile_t* ts_file);
void tsfile_backend_destroy(tsf_backend_t* backend);

STATIC_INLINE int tsfile_backend_set(tsf_backend_t* backend, const char* option) {
	return backend->tsf_class->set(backend, option);
}

STATIC_INLINE int tsfile_backend_get(tsf_backend_t* backend, int start, int end) {
	return backend->tsf_class->get(backend, start, end);
}

STATIC_INLINE int tsfile_backend_add(tsf_backend_t* backend) {
	return backend->tsf_class->add(backend);
}

#endif /* TSFILE_H_ */
