## Prerequisites 
* Python 2.6 or 2.7 (for SCons build system and various build scripts) - [www.python.org](http://www.python.org/).    
    Also you will need to install pywin32 extensions on Windows: [sourceforge.net/projects/pywin32/](http://sourceforge.net/projects/pywin32/)
* SCons build  2.1 or newer - [www.scons.org](http://www.scons.org/).    
* Compiler and linker
    * For Unix and Linux - GNU C/C++ Compiler 4.4. Proprietary Unix compilers such as IBM XLc and SunCC are currenly **not** supported
    * For Windows - Visual Studio 2010 Express (MinGW is not tested)
    
### Installing prerequisites on Linux

* Simply install them using your preferrable packaging system. Most Linux systems provide Python 2 out-of-box.
    * RHEL/OEL/CentOS/   
    `# yum install scons gcc binutils`   
    NOTE: that RHEL 5 ships GCC 4 in separate package called gcc-44
    * SLES/openSUSE   
    `# zypper in scons gcc binutils`
    * Debian/Ubuntu   
    `# apt-get install scons gcc binutils`
    
### Installing prerequisites on Solaris 11

* Install SCons from sources:    
    ```
    # wget 'http://prdownloads.sourceforge.net/scons/scons-2.2.0.tar.gz'
    # tar xzvf scons-2.2.0.tar.gz
    # cd scons-2.2.0/
    # python setup.py install
    ```
* Install additional packages from IPS:    
    ```
    # pkg install pkg:/developer/gcc-45
    # pkg install pkg:/system/header
    # pkg install pkg:/developer/build/onbld
    ```
    NOTE: onbld provides ctfmerge and ctfconvert utilities which are used to convert debugging information from DWARF to CTF format (latter is supported by Solaris Modular DeBugger). They are not necessary.

On Solaris 10 you will probably need to use [OpenCSW](http://www.opencsw.org/) repositories.

### Installing prerequisites on Windows

* Download and install Python: [www.python.org/download/](http://www.python.org/download/)
* Add Python directory to system's PATH:
    * Go to "Start" menu, then right click on Computer
    * Select "Properties" in drop down list
    * (on Windows 2008 and later) Press on "Advanced system settings" hyperlink
    * Select "Advanced" tab
    * Press "Environment Variables..." button
    * Find "Path" in System Variables, select it, click on Edit...
    * Append ";C:\Python27" (depending on installation) to PATH
    * Save your settings
* Install pywin32 extensions and SCons. First option is to install them using binary installers (.exe). However there is a bug in autodist, so installer couldn't detect python if you have been installed x86-64 python. In this case you will need download source, than run:    
    `C:\...\scons-2.2.0> python setup.py install`    
SCons installs to python root so there is no need for changing system's path.
* Install Visual Studio

## Building 

To build TSLoad agent you need to change your working directory to directory containing sources than run SCons:    
`# scons [options] [arguments] [targets]`

To list SCons options (both own SCons options and project options), use:   
`# scons --help`    

SCons supports parallel make:    
`# scons -j 8`    
NOTE: PLATAPI processor uses file locking to protect plat_cache database. It may hang build process.

To make SCons quiet, specify `-Q` option.

There are several targets provided by agent:
* **configure** - only configures build system, generates genconfig.h and genbuild.h
* if no target name provided, it builds agent from source
* **tests** - run TSLoad unit tests
* **doc** - generates documentation  
* **install** - builds agent and installs it to desired location (specified by --prefix option)
* **zip** - builds agent, installs it and makes zip file from that
* To clean up after build (analogue to make clean), run scons with `-c` option (you may also run `scons install -c`)

To build debug version of loader (currently it is built by default), you need to specify `--enable-debug` option. `--enable-trace` adds additional tracing facilities into loader. These options has `--disable-*` siblings that disable debug/trace options.

To generate custom tsload.cfg for agent you may specify `--tsload-log-file` and `--tsload-server` options.

## Testing

So let's make test build of our loader (this example is for Linux).

* Clone sources from GitHub. You may also use rsync to clone them from designated location.
* Change dir to generator-agent:   
    `# cd <...>/agent`
* Run SCons:   
    `# scons install`
* Change dir to default install directory (build/$TSPROJECT-$TSVERSION-$PLATFORM):   
    `# cd build/tsload-agent-0.2-linux2/`
* Check version of tsexperiment:   
    `# bin/tsexperiment -v`
* Try to run test experiment:   
    `# bin/tseexperiment -e var/tsload/sample run`
    