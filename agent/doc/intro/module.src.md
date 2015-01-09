### Writing your own module

As we mentioned in [introduction][intro/intro], TSLoad is a _modular_ framework, so all workloads it could generate and monitors it provide are implemented in several modules. _Module_ is an external _dynamic loadable library_ (or _shared object_ in Unix terminology.

#### Generating module sources

TSLoad has a special tool - [tsgenmodsrc][ref/tsgenmodsrc] that generates source code and build files for workload type from templates. It is described in documentation. However, it is recommended to read this page of documentation too to know what it generates.  

#### Writing module from scratch

When TSLoad agent starts, it loads every library in its module directory, calls `mod_config()` function from it and keeps it in memory while process is working. When process stops, it calls corresponding `mod_unconfig()`.

Let's take a closer look to a skeleton of TSLoad Module:

```
#define LOG_SOURCE "test_module"
#include <tsload/log.h>

#include <tsload/defs.h>
#include <tsload/modapi.h>

DECLARE_MODAPI_VERSION(MOD_API_VERSION);
DECLARE_MOD_NAME("test_module");
DECLARE_MOD_TYPE(MOD_TSLOAD);

struct module* self = NULL;

MODEXPORT int mod_config(struct module* mod) {
	self = mod;
	return MOD_OK;
}

MODEXPORT int mod_unconfig(struct module* mod) {
	return MOD_OK;
}
```
First two lines contain macro named `LOG_SOURCE` is a helper for logging routines so they all have same logging source, and a corresponding include. The next header `tsload/defs.h` contains common macro definitions used inside TSLoad, for example `boolean_t` type. It is recommended to put this include directive first, but not necessary. Following header `tsload/modapi.h` defines API between module and TSLoad core. 

After include directives, three `DECLARE_*` macro statements are going. They add global variables recognizable by TSLoad [][tsload/modules] subsystem. If they would be set to incorrect values, TSLoad will drop module thinking it is incompatible. 

	NOTE: TSLoad doesn't support module signing, and all modules are loading automatically. It may be a security vulnerability. 
	
Now, when module is written, it is time to build it! You may use your favorite build system for that, i.e. Make and its front-ends, or even manually run compiler and linker. Another option is to take [SCons](http://www.scons.org/) which is used internally in TSLoad. It allows to use some parts of TSLoad build scripts while building modules. For example, your module will have same CFLAGS TSLoad build with. 

Now let's write simple SCons build script (which is called `SConstruct`) that uses TSLoad development files:
```
import os

env = DefaultEnvironment()
env['TSLOAD_DEVEL_PATH'] = '/path/to/TSLoad/development/files'
env['TSEXTPATH'] = Dir('#').abspath

SConscript(os.path.join(env['TSLOAD_DEVEL_PATH'], 
							'SConscript.ext.py'), 'env')

env.Module('load', 'test_module')
```

First line contains simple Python module import and not interesting. In the following lines, _Environment_ is created - a global SCons associative arrays that contains build variables. Most of them are set internally by SConscripts, in our case we define only two of them - `TSLOAD_DEVEL_PATH` - that contains path to TSLoad development files and `TSEXTPATH` - path to our module sources (generated automatically based on SConstruct location). Development files are usually written to `share/tsload/devel` path inside TSLoad directory.
  
Then a file called `SConscript.ext.py` loaded. _SConscripts_ is a name for auxiliary build scripts in SCons, so this file configures environment for the module. As a part of this process it adds `Module` operation to it. The last line in our SConstruct file is call of `Module` with two parameters: type of module: (should always be `'load'`) and name of it. Note that we didn't specify source file location. Thats because `Module` method assumes that you have followed TSLoad [coding guidelines][devel/codestyle] and put source files into root directory of module and headers to `include` subdirectory.
 
After writing a module just say in a terminal: ` $ scons install ` and SCons will build and install module to an appropriate directory. But this module is completely useless. Let's see, how to add abilities to it.

#### Implementing a workload type

So we wrote a module, but its completely useless to us, we need to implement a workload type. As we mentioned before, TSLoad allows to parametrize both workloads and its requests, so we need to create two C structures - one holding workload parameters, and other - for request parameters. Because TSLoad will write to a raw memory (C doesn't have reflection), use only [wlp\_\* types][tsload/wlparam] for it:
```
struct test_module_workload {
	wlp_integer_t	some_parameter;
};
```
While TSLoad had bee configuring workload, it will allocate test_module_workload structure, write value for `some_parameter` from config to it and pass pointer to that structure in `wl_params` member of workload. Same works for a request - its parameters are saved in `rq_params` field of a request. Internal data of a workload can be saved in a `wl_private` field.

After that, create an array of [wlp\_descr\_t][tsload/wlparam#wlp_descr_t] structures, ending with WLP_NULL placeholder:
```
MODEXPORT wlp_descr_t test_module_params[] = {
	{ WLP_INTEGER, WLPF_NO_FLAGS,
		WLP_INT_RANGE(10, 20), WLP_NO_DEFAULT(),
		"some_parameter",
		"This parameter is only an example, set its value to whatever you want",
		offsetof(struct test_module_workload, some_parameter) },
	{ WLP_NULL }
};
```
See explanation of this in [wlp\_\* types][tsload/wlparam] documentation, or use tsgenmodsrc to generate it.

Functions!
```
MODEXPORT int test_module_wl_config(workload_t* wl) {
	return 0;
}
MODEXPORT int test_module_wl_unconfig(workload_t* wl) {
	return 0;
}
MODEXPORT int test_module_run_request(request_t* rq) {
	return 0;
}
MODEXPORT int test_module_step(struct workload_step* wls) {
	return 0;
}
```

First function, `*_wl_config` configures workload - i.e. making some preliminary actions that needed to correctly simulate it. I.e. it could be a calibration run or allocating disk storage. Note that each config function is called from separate thread, so if there are several workloads to be configured, they may compete for resources. Experiment wouldn't start until all workloads  If configuration takes a lot of time, add [wl\_notify()][tsload/workload#wl_notify] calls to it with an appropriate messages. If configuration fails, call `wl_notify` with `WL_CFG_FAILED` with the reason of it and return non-zero value. Second function `*_wl_unconfig` is opposite to it -
reverts changes made by workload.

Third function: `*_run_request` is a heart of all TSLoad. It is responsible for request simulation. This function is called from the context of worker threads in a [threadpool][intro/threadpool] and could theoretically run in parallel, so be careful with shared data. The last function `*_step` is called at the beginning of each step from a control thread of a workload. 

When you done it is time to acquaint TSLoad with your workload type. Create a [wl_type_t][tsload/wltype#wl_type_t] descriptor, and call `wl_type_register()` in `mod_config()` as well as `wl_type_unregister()` in `mod_unconfig()`:
```
wl_type_t test_module_wlt = {
        /* wlt_name */                  AAS_CONST_STR("test_module"),
        /* wlt_class */                 WLC_CPU_INTEGER | WLC_CPU_MISC,
        /* wlt_params */                test_module_params,

        /* wlt_params_size*/    sizeof(struct test_module_workload),
        /* wlt_rqparams_size*/  0,
        /* wlt_wl_config */             test_module_wl_config,
        /* wlt_wl_unconfig */   test_module_wl_unconfig,
        /* wlt_wl_step */               test_module_step,
        /* wlt_run_request */   test_module_run_request
};
```

As you can see, we set `wlt_rqparams_size` to zero, because our workload had no request parameters, so `rq_params` would be set to NULL. This descriptor also contains `wlt_class` field that classifies workload by resource it gathers - i.e. with `WLP_CPU_INTEGER | WLP_CPU_MISC` is a processor-bound and ALU-bound workload.
 