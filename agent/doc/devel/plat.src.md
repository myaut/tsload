## Platform-specific builds

TSLoad supports multiple platforms: Solaris, Linux and Windows. The thing is that some of the features are shared between them via POSIX standard, some features are present in both Solaris and Linux, etc. To overcome such situations in C, messy code with tons of `#ifdef`s is used. However, TSLoad buidl system provides its own preprocessor called __PlatAPI__. 

PlatAPI uses special environment variable called `'PLATCHAIN'` that contains list of platforms that current platforms implements. It is set based on Python `sys.platform` variable as set in `SConscript.plat.py`:

---
__Platform__ | `sys.platform` __value__ | __Platform chain__
Windows | `win32` | win, generic
Linux | `linux2` | linux, posix, generic
Solaris | `sunos5` | solaris, posix, generic
---

Platform-specific sources are located in `plat/NAME` where NAME is a name from a platform chain. PlatAPI walks over platform chain, and searches for a best-match implementation. __Header__ files are installed in `<inc_dir>/plat/include.h` as is: for example if `plat/linux/file.h` and `plat/posix/file.h` are exist, for Linux it will pick `plat/linux/file.h`, for Solaris it will copy `plat/posix/file.h` and for Windows no header file is copied.

__Source__ files are parsed and preprocessed: best implementation is chosen on function-level. PlatAPI parses source and header files to choose it as described in following paragraph.

### PlatAPI parser

PlatAPI parses global header files in `include/` and selects function declarations with `PLATAPI` qualifier. That qualifier is purely for PlatAPI, it is defined as macro in `tsload/defs.h`:

```
#define PLATAPI
#define PLATAPIDECL(...)
```

These function declarations are put into `plat_cache.pch` file (which actually a Python pickled `dict`). Note that when `scons` is called with `-j` option, `parse-include` will run in parallel, so to protect cache file, it is locked using filesystem routines. After that `proc-source.py` is started and processes sources according to platform chain. When appropriate implementation is found, `proc-source.py` saves function name into `plat_cache.pch`. Function is removed from all subsequent platform sources based on braces.

There is also macro `PLATAPIDECL` that discards declaration if corresponding functions were removed from sources. Names of functions that use such declarations are passed as arguments to `PLATAPIDECL`. 

Finally, if there is no way to get rid of `#ifdefs`, PlatAPI defines `PLAT_NAME` macro where NAME is a upper-cased name of platform from platform chain. 

### Example

Let us implement cross-platform file access API using PlatAPI:

iofile.h
```
#ifndef IOFILE_H_
#define IOFILE_H_

#include <plat/iofile.h>

/* This declaration is parsed by parse-header */
PLATAPI int plat_open_file(plat_file_t* f, const char* path);

#endif
```

iofile.c
```
/* Use path without specifying platform name. When TSLoad is built, 
 * best matching header file is automatically copied to include/ directory. */
#include <plat/iofile.h>

/* Uses PLAT_* macro to check if source is built on POSIX-compatible
 * platform. If not, iof_open_flags is omitted. */
#ifdef PLAT_POSIX
extern int iof_open_flags;
#endif

int iof_init(void) {
#ifdef PLAT_POSIX
	tuneit_set_int(int, &iof_open_flags);
#endif
	return 0;
}
```

plat/posix/iofile.h
```
#ifndef PLAT_POSIX_IOFILE_H_
#define PLAT_POSIX_IOFILE_H_

/* Platform-specific structure declaration. May be used as is
 * or as a part of a compound struct. */
typedef struct { int fd } plat_file_t;

#endif
```

plat/posix/iofile.c
```
/**/
#include <plat/iofile.h>

/* Only compiled when plat_open_file from POSIX implementation is used. */
PLATAPIDECL(plat_open_file) int iof_open_flags;

/* This function is available on POSIX-compatible platforms (i.e. in Linux code)
 * but will never be compiled on Windows. If you need universal Windows-POSIX code,
 * put it into "generic" platform. */
int some_helper(int fd) {
    return -1;
}

PLATAPI int plat_open_file(plat_file_t* f, const char* path) {
	/* Call open() */
}
```

plat/windows/iofile.h
```
#ifndef PLAT_WINDOWS_IOFILE_H_
#define PLAT_WINDOWS_IOFILE_H_

#include <windows.h>

/* Another platform-specific struct definition: instead of 
 * integer file descriptors, it uses Windows Handles. */
typedef struct { HANDLE h } plat_file_t;

#endif
```

plat/windows/iofile.c
```
#include <plat/iofile.h>

PLATAPI int plat_open_file(plat_file_t* f, const char* path) {
	/* Call CreateFile() */
}
```



