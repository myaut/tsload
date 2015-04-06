## Build system

TSLoad uses [SCons](http://www.scons.org/) as its build system, mostly because CMake asked $30 for book as a documentation. Other advantage (and in a same time a disadvantage) is that is written in Python and very extensible. Over time TSLoad build system incorporated many extensions, whihc will be discussed further.

SCons is configured through series of __SConscript__ files and a main file called __SConstruct__. There are two types of SConscript files that are used in TSLoad's agent SConstruct: file located in `path/to/component` is responsible for building that particular component, while file named as `SConscript.xxx.py` which is located in a root of agent directory is a helper to SConstruct that extends its functionality. Some of these helpers are used for [building modules][intro/module] and installed as part of `tsload-devel` package. Some extra python modules are located in `tools/build`.

TSLoad agent building consists of several steps as described in __SConstruct__ file:
 * _Defining build options and generating environment._ Environment helpers and variable values are set in `SConscript.env.py`. Variables related with installation and builders for that is set up in `SConscript.install.py` after configuring is done. [Platform-specific][devel/plat] builders are loaded from `SConscript.plat.py`.
 * _Configuring TSLoad._ Like _autoconf_, SCons checks various capabilities of system's API and compiler. This step also responsible for generating header files. 
 * _Building TSLoad._ TSLoad walks over list of its components (libraries, binaries, modules) and calls lower-level SConscript files for each component. These SConscripts usually do the following:
    * Pre-processing of some files i.e. by using [ETrace][tscommon/etrace]. ETrace helpers are provided by `SConscript.etrace.py` helper
    * Build and install executable files 
    * Install additional files, including development files such as includes
    * For libraries -- build documentation files
 * _Building documentation_ using [TSDoc][devel/tsdoc]. It is defined in `doc/SConscript` file while some helpers are provided by `SConscript.doc.py` helper.
 * [Running tests][devel/tsdoc] as defined in `tests/SConscript`.
 * _Building packages_.
 
 There is one special helper -- `SConscript.ext.py`. It is not used by SConstruct of TSLoad agent, but provided for external SConstructs, i.e. for modules. 
 
TSLoad uses SCons [variant dirs](http://www.scons.org/doc/production/HTML/scons-user.html#idp14616296), so all build output files are put into `build/` subdirectory.
 
### Construction environment
 
To maintain build configuration and builders SCons uses global object called [construction environment](http://www.scons.org/doc/production/HTML/scons-user.html#sect-construction-environments). It is created in SConstruct file and passed to SConscripts using `Import('env')` (object is globally called as `env`). Helper SConscripts should modify `env` and export its changes back with `Export('env')`. Component SConscripts that alter environment should clone it and name resulting environment: `lib = env.Clone()`. Note that copy is called `lib` -- to improve readability, they are colled according to component type: `lib` - shared library, `cmd` - command, `tst` - test.
 
#### Variables 
 
To keep various TSLoad settings, environment variables are used. There are list of them:

 * __TSLoad project settings__.
   * `TSLOADPATH` -- path to TSLoad sources. For agent builds should be set to abspath of `'#'` (hint for SCons to use top-level path of project). For external builds automatically set from path to development files which is taken from variable: `TSLOAD_DEVEL_PATH`.
   * `TSPROJECT` and `TSVERSION` -- name of the project and current version
   * `TSNAME` -- full name of TSLoad build including platform
   * `PREFIX` -- root directory where TSLoad will be installed. If not set, defaults to `build/$TSNAME`
   * `INSTALL_*` -- subdirectories for installation of TSLoad components. See [installation][devel/build#installation] below. Values are defined in `tools/build/installdirs.py`
 * __Configuration options__. Most of them are gettable via `GetOption('option_name')` but some of them have extra handling and provided by environment itself:
   * `DEBUG` -- enables debug build
   * `LOAD_LOGFILE` and `TSLOAD_MODPATH` -- path to tsload logging and tsload modules path (for generation of sample tsloadd config)
   * `ETRACEENABLED` -- enables [ETrace][tscommon/etrace] processing. Enabled through `--enable-usdt` and `--enable-etw` options depending on platform.
   * `VERBOSE_BUILD` -- internal hint for TSLoad that disables pretty printing (if `'cmdline'` is in list) or passes verbosity flags to . Taken from `--verbose-build` command line option.
 * __Compiler and linker settings__.
   * `CC` and `CCFLAGS` -- C compiler and its flags
   * `LINK` and `LINKFLAGS` -- static linker and its flags
   * `SHCCFLAGS`, `SHLINKFLAGS` -- same as previous, but provides specific options for shared objects
   * `CPPPATH` and `LIBPATH` -- paths to include files and libraries 
   * `LIBS` -- names of libraries (should be altered by components)
   * `TARGET_ARCH` -- name of target build architecture
 * __Global state__. Some variables are used to save SCons objects across SConscript calls. Some of they will be described in corresponding sections of this manual. 
   * `GENERATED_FILES` -- contains generated include files to be installed as a development files 
   
#### Helpers

TSLoad build sytem provides several build helpers that handle some extra activities for component and simplifies writing component SConscripts:

```
PathJoin(path, *paths)
PathSplit(path)

PathBaseName(path)
PathDirName(path)

PathExists(path)
PathIsAbs(path)
PathIsFile(path)

PathAbsolute(path)
PathRelative(path[, start])

PathWalk(top, topdown=True, onerror=None, followlinks=False)

PathAccess(path, mode)
```

Aliases for `os.path` and `os` that make them look conformant with SCons all-capitalized style. 

```
PathRemove(abspath, path)
PathRemoveAbs(abspath, absdir)
```

Remove part of absolute path `abspath`: from beginning (`PathRemoveAbs`) or tail (`PathRemove`). If it is not possible (i.e. when `absdir` is not parent to `abspath`), raises an exception.

```
env.BuildDir(dir, subdir='build')
```

Returns absolute path to a temporary build dir subdirectory identified by `dir` and `subdir`: `PathJoin(root_path, 'build', subdir, dir)`

```
env.Macroses(*macroses)
```

Add definitions in form of list of strings `'MACRO_NAME=value'` or `'MACRO_NAME'` in compiler-independent way.

```
env.CompileSharedLibrary(extra_sources = [], ctfconvert = True)
```
Compiles shared library using `*.c` sources from component root directory, `*.c` files from platform directories. If extra files need, pass them as `extra_sources` list. Returns list of compiled objects.

`ctfconvert` is option for Solaris and it enables converting DWARF to CTF.

```
env.CompileProgram(extra_sources = [])
```

Compiles a program objects. Same rules for picking source files as for `CompileSharedLibrary` apply here.  Returns list of compiled objects.

`CompileProgram` invokes two additional builders:
  * `UsageBuilder` generates `usage.c` file which `usage(int retcode)` function (written in C) from `usage.txt` file.
  * `SubsysBuilder` generates subsystem initializer/finalizer file `init.c`
  
```
env.LinkProgram(target, objects)
```

Builds program named `target` from list of objects `objects`. Returns built program.
  
```
env.LinkSharedLibrary(target, objects, ctfmerge = True)
```

Builds shared library named `target` from list of objects `objects`. Returns built program.

`ctfmerge` is similiar to `ctfconvert` option and only valid on Solaris.

```
env.Module(mod_type, mod_name, etrace_sources = [])
```

Compile, build and install module. `mod_type` should be set to `'load'`, `mod_name` is a name of module. `etrace_sources` contains list of sources that need to be preprocessed with ETrace. Returns built module.

```
env.InstallTarget(tgtroot, tgtdir, target):
```

Installs file. `tgtroot` is a name of package to which this target belongs. `tgtdir` is a target installation directory (base directory should be taken from `env['INSTALL_*']`). 
  
### Subsystems 

Many TSLoad agent functions use some global state such as memory SLAB cache, mutex locks, global hashmap or list. I.e. `wl_type_register()`, which is used in modules, adds workload type to a global hashmap `wl_type_hash_map`. So before `mod_init()` is called, we should initialize that hashmap with `wtl_init()` - subsystems have dependencies. 

Every TSLoad agent executable should do that at beginning, so it calls `ts_init()` function (see: [init][tscommon/init]). To simplify management of dependencies, and automatically fill list of subsystems passed to `ts_init()`, build system provides `env.UseSubsystems()` call which accepts names of subsystems. When executable is built, it generates necessary objects and `init()` function which serves as a wrapper to `ts_init()` into `init.c` file. `env.UseSubsystems()` also adds libraries to linked with executable.

Subsystems and their dependencies list is located in `lib/subsystems.list` file.

### Component SConscript sample

Now lets take a look at sample of component SConscript (actually, for `tsgenmodsrc` binary):

```
1  from pathutil import *
2  
3  
4  tgtroot = 'tsload-core'
5  target = 'tsgenmodsrc'
6  
7  Import('env')
8  
9  cmd = env.Clone()
10 
11 cmd.UseSubsystems('log', 'mempool', 'json', 'modsrc')
12 
13 objects = cmd.CompileProgram()
14 tsgenmodsrc = cmd.LinkProgram(target, objects)
15 
16 cmd.InstallTarget(tgtroot, cmd['INSTALL_BIN'], tsgenmodsrc)
17 cmd.InstallTarget(tgtroot, PathJoin(env['INSTALL_VAR'], 'modsrc'), File('modinfo.json'))
18
19 for modsrc_file in Glob('modsrc/*'):
20     cmd.InstallTarget(tgtroot, PathJoin(env['INSTALL_SHARE'], 'devel', 'modsrc'), modsrc_file)
```

 * Line 1: `pathutil` module is imported to get `PathJoin` alias.
 * Lines 4-5: These are arguments to `env.LinkProgram()` and `env.InstallTarget()`. To simplify identification of SConscript, they are put on the top of the file.
 * Lines 7,9: Environment is imported from `SConstruct` and cloned into new `cmd` object
 * Line 11: `env.UseSubsystems()` provides list of used subsystems, so `LIBS` variable is updated with neccessary subsystems: libtscommon will be used for 'log' and 'mempool', libtsjson for 'json' and 'modsrc' is internal tsgenmodsrc subsystem.
 * Further alterations of `cmd` may be done here. I.e. system libraries is added to `LIBS` here. 
 * Lines 13-14: `tsgenmodsrc` binary is built here
 * Line 16: this binary is installed to default binaries path. 
 * Line 17, 19-20: some extra files are installed. Note that for directories we have to glob each file: if we just call `env.InstallTarget()` for directory, SCons won't track changes of files and don't reinstall them. You can also add dependency between targets so if one file is installed, other will be replaced too.
 
As we said, TSLoad have many customized builders. One example of them is a "experiment errors builder" which generates list of experiment error codes and their nicnames from `experr.h` file for `tsexperiment`:

```
import sys

...

ExpErrGenBuilder = Builder(action = Action('%s $TSLOADPATH/tools/genexperr.py $SOURCE > $TARGET' % (sys.executable),
                                           cmd.PrintCommandLine('GENEXPERR')))
cmd.Append(BUILDERS = {'ExpErrGenBuilder': ExpErrGenBuilder})
experr_c = cmd.ExpErrGenBuilder('experr.c', 'include/experiment.h')

...

objects = cmd.CompileProgram()
tsexperiment = cmd.LinkProgram(target, objects)
```

Since SCons is a Python program itself, we use path to Python interpreter to call `genexperr.py`. That script accepts name of source file as first argument and puts resulting source to STDOUT, so we have to use shell output redirection to put source into target file. Second argument to `Action()` call is command line format string -- it is generated by `env.PrintCommandLine()` so for pretty command line output it will write 'GENEXPERR source_name' to console instead of full command line.

After we defined a builder, we invoke it directly to build 'experr.c' source file from 'include/experiment.h' header (which will be `$TARGET` and `$SOURCE` correspondingly). This however, doesn't cause `genexperr.py` to be immediately called. Instead, SCons saves 'experr.c' in its virtual filesystem representation marking its dependencies and builder. When `env.CompileProgram()` is called afterwards, it globs directory including virtual filesystem, so it adds 'experr.c' to a list of source files and creates new dependency: experr.o -> experr.c. Finally, `env.LinkProgram()` adds further dependencies. 

### Installation 

[installation]

By default all TSLoad files are installed into `build/$TSNAME` subdirectory, but this behavior can be altered by specifying `--prefix` command line option to SCons (as it does in autoconf). For example, it can be set to `/usr/local`, `/opt/tsload` or `C:\Program Files\TSLoad`. 

However, depending on a nature of installing file, it would be put to a appropriate subdirectory: i.e. executable files on Unix-like systems are put into `bin/` subdirectory, but on Windows they preferred to be put in a top-level installation directory. To reflect that difference, TSLoad uses environment variables `INSTALL_*` which are defined in `tools/build/installdirs.py` helper file:

---
__Environment variable__ | **UNIX subdirectory** | **Windows folder** | **SCons option** | **Description**
`INSTALL_BIN` | bin/ | .\ | --bindir | binary files
`INSTALL_LIB` | lib/ | .\ | --libdir | shared libraries
`INSTALL_ETC` | etc/ | Configuration\ | --etcdir | configuration files
`INSTALL_INCLUDE` | include/ | Include\ | --includedir | include files (headers)
`INSTALL_VAR` | var/tsload/ | Data\ | --vardir | variable files
`INSTALL_SHARE` | share/tsload/ | Shared\ | --sharedir | shared files
`INSTALL_MOD_LOAD` | lib/tsload/mod-load/ | LoadModules\ | --loadmoddir | loader modules
---

Note that internally TSLoad uses that subdirectories to deduce where its files located -- i.e. logs are put into `var/tsload/log` (unless `TS_LOGFILE` is specified). So it is not recommended to move or rename subdirectories, instead rebuild with different `--vardir`.

