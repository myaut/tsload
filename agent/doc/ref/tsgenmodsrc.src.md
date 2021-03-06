## tsgenmodsrc

##### Name
tsgenmodsrc - Generate source code for TSLoad module

##### Synopsis
`$ tsgenmodsrc [global options] subcommand path/to/modinfo.json`

##### Description
tsgenmodsrc generates environment using [modinfo.json][ref/modinfo_json] configuration file, called _modvars_. Then it loads templates from development directory of TSLoad and preprocesses them.

A chosen set of generated files depends on __-b__ and __-t__ global options:
 * __-b__ with an option argument:
   * `scons` - Generate build files for SCons (default)
   * `make` - Generate build files for GNU Make
 * __-t__ (only valid with -b scons)
   * `int` - Generate SConscript to be used inside TSLoad
   * `ext` - Generate SConstruct to build module outside (default)

Paths to TSLoad installation directories that is used by _tsgenmodsrc_ to locate templates and added to build files are usually deduced from tsgenmodsrc location, but if they didn't, they could be specified from environment variables:
 * `TSLOADPATH` - root directory where TSLoad is installed
 * `TS_INSTALL_SHARE` - path to share directory relative to TSLOADPATH
 * `TS_INSTALL_INCLUDE` - path to include directory relative to TSLOADPATH
 * `TS_INSTALL_LIB` - path where libraries are installed relative to TSLOADPATH
 * `TS_INSTALL_MOD_LOAD` - path where modules should be installed relative to TSLOADPATH

### Variables

To see what variables are generated by _tsgenmodsrc_, call it with __vars__ subcommand:
```
$ tsgenmodsrc [global options] vars path/to/modinfo.json
```

These options may be redefined by `vars` object inside modinfo.json config. However some of them has `[generated]` flag that means that they are not settable from config, and generated from other options - for example structure fields.

  * `MODNAME` - name of module, taken from configuration file 
  * `MODTYPE` - type of module, taken from configuration file
  * `SOURCE_FILENAME`, `HEADER_FILENAME` - name of source files
  * `MAKEFILE_FILENAME`, `SCONSTRUCT_FILENAME`, `SCONSCRIPT_FILENAME` - names of build files. It is advised not to change these variables.
  * `MODULE_FILENAME`, `OBJECT_FILENAME` - build file names
  * `HEADER_DEF` - macro defined inside header in #ifdef / #define construct
  * `CCFLAGS`, `LIBS`, `SHCCFLAGS`, `SHLINKFLAGS` - compiler and linker flags (taken from build environment of TSLoad)
  * `SHOBJSUFFIX`, `SSHLIBSUFFIX` - filename suffixes (extensions) for modules (taken from build environment of TSLoad)
  * `MODINFODIR` - directory where _modinfo.json_ is located
  * `TS*` - same as environment variables (see above)

Workload type variables are contained in namespaces, so different wltype would get different values:
  * `WLT_NAME` - name of workload type, usually matches with name of the module
  * `WLT_ABBR` - abbreviation of workload type which is needed to generate variable names. Contains first letter of WLT\_NAME followed by letters after underscores. I.e. test\_module -> tm.
  * `WL_PARAM_STRUCT`, `WL_PARAM_FIELDS`, `WL_PARAM_VARNAME` - Workload parameters. STRUCT contains name of data structure keeping them, FIELDS is a generator for C structure members, and VARNAME is a name of variable that would be created in functions implementing workload type. 
  * `WL_DATA_STRUCT`, `WL_DATA_VARNAME` - Same as WL\_PARAM\_\* but for internal workload data.
  * `RQ_PARAM_STRUCT`, `RQ_PARAM_FIELDS`, `RQ_PARAM_VARNAME` - Same as WL\_PARAM\_\* but for per-request params.
  * `WLT_VARNAME` - name of workload type descriptor variable
  * `WLT_CLASS` - generated from config - class of workload
  * `WL_PARAMS_VARNAME` - name of workload parameter descriptor array
  * `WL_PARAMS_ARRAY` - generated from config - workload parameter descriptors
  * `FUNC_CONFIG` - function name of workload configuration
  * `FUNC_UNCONFIG` - function name of workload de-configuration
  * `FUNC_RUN_REQUEST` - name of the function that runs request
  * `FUNC_STEP` - name of the function that is called at the beginning of each step
  
### Generating sources

Sources are generated in two modes. In first one, _tsgenmodsrc_ will write appropriate file to stdout, where template used for file is determined by provided subcommand:
 * `header` or `hdr` will generate C header
 * `source` or `src` will generate C source
 * `makefile` will generate build file which is determined by __-b__ and __-t__ options arguments.
 
In second mode, which is activated by `generate` command, _tsgenmodsrc_ will automatically create file hierarchy inside the same directory where _modinfo.json_ configuration file is located. If directory has other files than _modinfo.json_, command will fail, so _tsgenmodsrc_ wouldn't overwrite them.  

If sources were generated in second mode, they and object files created during build may be cleaned up with `clean` command. _tsgenmodsrc_ caches creation times of files in special file `.ctime_cache`, so if you will change source, it will warn you before cleanup. `clean` subcommand will ask for confirmation before cleanup that should be answered `y` (yes) or `f` (force), if warnings were found.

### Preprocessor

To generate sources and build files, _tsgenmodsrc_ uses preprocessor. This preprocessor uses at symbol `@` as command character. Note that it doesn't support its escaping (but for Makefiles where `$@` has special meaning), it will ignore that character. 

Preprocessor supports following features:
* Substitution: variable surrounded by ats replaced with its value or generated text. I.e. `@WLT_NAME@`
* Directive `@ifdef` followed by variable name. Conditional generation - adds following lines to resulting file only if variable is defined. May be followed by `@else` block, ended with `@endif` directive. 
* Directive `@define` followed by variable name. Creates new macro, that supports substitution like variables. The following text may contain macro-substitutions or conditional directives. Ended with `@enddef` directive. These directives couldn't be nested.

Complete BNF for preprocessor is:
```
<opt-whitespace>     ::= <whitespace> | <whitespace> <opt-whitespace> | ""

<var-char>           ::= <alpha> | "_"
<var-name>           ::= <var-char> | <var-char> <var-name>

<dir-param-var>      ::= <opt-whitespace> <var-name> <opt-whitespace> <EOL>
<dir-param-nothing>  ::= <opt-whitespace> <EOL>

<subst-expr>         ::= "@" <var-name> "@"

<block>              ::= <text> | <ifdef-block> | <subst-expr>
<blocks>             ::= <block> | <block> <blocks>

<else-block>         ::= "@else" <dir-param-nothing>
                         <blocks> <EOL>
<opt-else-block>     ::= <else-block> | ""
<ifdef-block>        ::= "@ifdef" <dir-param-var>
                         <blocks> <EOL>
                         <opt-else-block>
                        "@endif" <dir-param-nothing>

<define-block>       ::= "@define" <dir-param-var>
                         <blocks>
                        "@enddef"
<foreach-block>		::= "@foreach" <dir-param-var>
						<blocks>
						"@endfor"                        
```
