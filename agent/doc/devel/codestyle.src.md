## Coding guidelines

__TSLoad__ agents are written in C, while server is written in Python. This document covers guidelines for agent code. Coding style was inspired by Solaris mostly.These rules are not strict, but recommended and followed by most of the code of TSLoad. 

### Basic coding guidelines

__Line length__. There are no limits for line lengths. Common, we live in an era of Full HD displays. But also note that long lines makes code hard to comprehend.

__Indentation__. four character block indent using tabs with additional spaces where needed. 
   * _Case_s have same indentation as corresponding _switch_. 
   * When you break function declaration and call into separate lines, arguments should be aligned to a left brace:
 ```
        json_set_parser_error(parser, JSON_END_OF_BUFFER,
                              "Unexpected end of buffer while parsing %s",
                              (is_array)? "ARRAY": "OBJECT");
 ```
   * When you break if- or while- condition into separate lines, indent should go deeper than corresponding block
   
__Curly braces__. TSLoad code follows 1TBS style with a small exception: `else` keyword is put onto new line

__Pointer declaration__. Put asterisk sign (`*`) closer to a type. If you want to make several pointers, declare them one per line.   

__Spaces__. Surround operators like `<` and `=` with spaces. Do not put spaces around braces. Put single space after commas as punctuation rules require.

__Return value checking__. It is OK to put assignment of return value variable to a function call result and check it in one if:
 ```
    if ((dladm_status = hi_net_sol_dladm_open(&dld_handle)) != DLADM_STATUS_OK) {
		hi_net_dprintf("hi_net_probe: dladm open failed: %d\n", dladm_status);
		return HI_PROBE_ERROR;
	}
 ```
 
__ifdefs__. Avoid ifdefs in function bodies: use [PLATAPI][devel/plat] where possible or create separate subsets of functions. 

__goto__. It is OK to use `goto` keyword in one case: you need to cleanup before you will return error code.

__Error handling__. Preferable way to handle errors is to return `int` that represents error code. For functions returning pointers it should be `NULL`. Each subsystem defines its own subset error codes using macro-definitions starting with NAME_OK = 0, and following NAME_ERR_... values that are negative. For subsystems which save error messages or additional data for errors, there should exist thread-local errno structure and API for accessing it.

### Source tree organization

Each component of TSLoad: library or a binary consists of smaller pieces called _subsystems_. Usually front-end of such subsystem is a data structure and operations applicable to it (which could be recognized as class). Subsystem is defined in _main header_ file placed under `agent/include/<library subdirectory>/<subsystem name>.h`. It can also place its implementation details into separate header located in `agent/<path-to-component>/include/<subsystem name>.h`. Platform-independent implementation is placed into source file `agent/<path-to-component>/<subsystem name>.c`, while its platform-specific code (both headers and sources) is placed in `plat` subdirectory:  `agent/<path-to-component>/plat/<platform-name>/<subsystem name>.[ch]`. 

For example, [threads][tscommon/threads] implementation consists of the following files:

 * `agent/include/tsload/threads.h` - global data definitions like `thread_t` or `mutex_t`, and operations with them: `t_join` and `mutex_unlock`. 
 * `agent/lib/libtscommon/threads.c` - platform-independent implementation for `thread_t` operations
 * `agent/lib/libtscommon/tutil.c` - platform-independent implementation for mutexes, condition variables, events and thread-local storage (they called thread key in TSLoad)
 * `agent/lib/libtscommon/plat/posix/threads.h` - POSIX definitions for threads and synchronization mechanisms, that are typedefing _libpthread_ types that makes them usable in global headers. I.e. `thread_t` type contains `t_impl` of type `plat_thread_t`, which is aliased name for `pthread_t`, and this file contains such typedef.
 * `agent/lib/libtscommon/plat/posix/threads.c` - POSIX implementation for `thread_t` operations
 * `agent/lib/libtscommon/plat/posix/tutil.c` - POSIX implementation for synchronization mechanisms


### Source structure 

Structure of main header goes as follows:

 * Header
  * GPL preamble with copyright.
  * #ifdef/#define from header name ending with \_H\_. Platform-specific headers should contain PLAT_<platform name> prefix
  * #include directives
  * TSDoc header for subsystem with `@module` helper
 * Constants, types and shared data
  * Constants defined with macros, i.e. size of statically allocated arrays, default tuneable values
  * Shared data with `LIBIMPORT` prefixes
  * Hooks types definitions and `LIBIMPORT` functions pointers to hooks
  * Types definitions and #define-based enumerations. C types should be typedefed with its typename ended with
 * Functions and macros that are work as functions
  * Operations applicable to that types grouped by its purpose. Groups are separated with one empty line with a comment on top of group, that is needed 
  * Global functions, including `_init()/_fini()` functions (see below).
  * Functions that build TSObject from subsystem main type or vice versa
 * Tailer 
  * #endif followed by a newline
  
Structure for source files is similiar:

 * Header
  * GPL preamble with copyright.
  * #include directives
 * Global data definitions
 * Function implementation with the same rules as for headers
 
### Naming conventions 

Because C doesn't support namespaces, which is actually useful feature and allows to distinguish functions and types, we "emulate" it by using prefixes: _each name should begin with subsystem name_. Subsystem name may be abbreviated or shortened. Naming conventions are similiar to GNU C.

#### Types

All types names should be lower-cased with underscores.

 * Each type should have typedefed alias that has suffix `_t`, except for function pointers that should end with `_func`, the name should be lower-cased with under scores. 
 * Enumerations (both typed and define-based) should have upper-cased names with underscores. 
 * (_Optional_) Structure member names should have prefixes that match structure name or its abbreviation.
 
#### Macro names

Generally speaking, macros should be named upper-cased with underscores, but there are couple exceptions for it  

 * Macro that define constants may not have underscores. Usually they are size related use upper-score abbreviation without underscores - these names should end with `SIZE` or `LEN`.
 * Some macros should be lower cased:
   * Foreach-like helpers should be lower-cased.
   * Function aliases that set default parameter values and should look-like functions should share name with it and through it are lower-cased.
   * Functions that should be type generics, but due to lack of that capability in C, became macros should be lower-cased, i.e.: [list operations][tscommon/list].
   
#### Function names   

All function names should be lower-cased. Function name consists of several components that are separated by underscores:

 * Prefixes
  * `plat` - prefix for platform-specific functions (optional). I.e. `mod_load_symbol` and `plat_mod_load_symbol`.
  * Subsystem or object name.
 * Operation name.
 * If operation is working on distingushable part of the object, name of it. I.e. workloads are consist of requests, so there are `wl_create_request` function that creates request and inserts it into workload list. Part name and operation name could be switched, so it's opposite called `wl_request_destroy`.
 * Suffixes
  * `byX` for specifying criteria of search. I.e. `sched_policy_find_byname`.
  * `nolock` for special functions that are not locking necessary mutexes. I.e. `hash_map_find` which calls `hash_map_find_nolock`.
  * `impl` for functions that have front-end and implementation. I.e. `wl_destroy` only simulates destruction, reducing reference count by 1, and when all references to function will be gone, last `wl_rele` calls `wl_destroy_impl` that will actually release resources.
  * For generic functions that have multiple implementations depending on nature of the object: "class derivatives" or functions overloaded with standard types it may have - name of type or derivative. I.e. `rg_generate_int_lcg` and `rg_generate_int_libc`, `json_as_*` functions

There are some conventions for operation names:

 * If function creates a new object by allocating its memory and setting its field making it fully usable after calling that function, it should be named `create`
 * If function initializes pre-allocated memory for object it is called `init`
 * If function de-allocates memory where possible, and act as destructor, it is called `destroy`
 * Functions that are initialize and deinitialize global state of a whole subsystems are called `init`/`fini`
 * Functions that iterate over sequence and call function for each element are called `walk`
 * Functions that increase reference counter called `hold` while opposite function called `rele` (for release).
 * Functions that process external representation of object coming from system or TSObject/JSON and create internal TSLoad object are called `proc`
 * Functions that create TSObject/JSON representation for TSLoad object are called `format`  
 * Getters are called `get` and setters are called `set` 

#### Variable names 

Global variable names lower-cased with underscores with a subsystem prefix. 

There are no clear rules for local variable or arguments naming, but there are several recommendations:

 * Use meaningful names for indexes. `i` acceptable only for short functions where loop is almost everything , `j` and `k` are not acceptable (except for matrix handling). `strandid` is almost good name, but strand is a unique for Fujitsu SPARC terminology and actually references _hardware thread_.
 * There are usually a "main variable" which is close to C++ `this`. Use same abbreviation for its name as for function naming. I.e. `workload_t* wl`
 * Good names for return value are `ret`, `err`.
 * Despite va_list variables usually called `ap`, they are called `va` in TSLoad code.  
 
### Comments
 
Since TSLoad agent is written in C and even simple actions require multiple syntax constructions, it is recommended to comment such actions.
 
 * Write [TSDoc][devel/tsdoc]
 * Comment all the magic and sorcery you bring into code. Magic constants doesn't need to have separate macro, but should have good comment, i.e. pre-calculated buffer sizes:
  ```
  /* 7 chars are enough to store thousands + suffix + space + null-term */
  ...
		if(length > (size - 8))
			return 0;
  ```
  * If function is complex and consists of several steps, start each step with its description in short comment.
  * Comment source code parts that are handling situation that arises outside of current thread control flow. I.e.
    * Multi-threaded code
  ```
  		/* Somebody allocating or freeing page now, wait on mutex than return
		 * last allocated heap page. If last_heap_page will be freed, page
		 * would be set to NULL, and we loop again */
  ```
    * Platform-dependent code
  ```
  		if(errno == ENOTSUP) {
				/* In Solaris 10, psets may be disabled if pools are not configured.  */
				return SCHED_NOT_SUPPORTED;
		}
  ```
  * If you have taken code or algorithm from Internet, add URL to a comment 