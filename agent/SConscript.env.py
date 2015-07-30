import sys
import os
import re

import traceback

from itertools import product
from pathutil import *
from subsys import SubsystemList 
from installdirs import debian_package_name

from SCons.Action import CommandAction, ActionFactory
from SCons.Node.FS import Base

Import('env')

def GetSConscriptPath(self):    
    modpaths = []
    absdir = Dir('#').abspath
    
    for modpath, _, method, _ in traceback.extract_stack():
        if method == '<module>':
            try:  
                modpaths.append(PathRemoveAbs(modpath, absdir))
            except ValueError:
                continue
    
    return modpaths[-1]

def Macroses(self, *macroses):
    prefix = '/D' if self['CC'] == 'cl' else '-D'
        
    for macro in macroses:
        self.Append(CCFLAGS = [prefix + macro])
    
def SupportedPlatform(self, platform):
    return platform in self['PLATCHAIN']

def BuildDir(self, dir, subdir='build'):
    root_path = self['TSLOADPATH'] if self['_INTERNAL'] else self['TSEXTPATH']
    
    return PathJoin(root_path, 'build', subdir, dir)
 
def AddDependency(self, dir, dep):    
    # there is no build directory for external builds, ignore it
    if self['_INTERNAL']:
        inc_dir = PathJoin(dir, dep, 'include')
        lib_dir = PathJoin(dir, dep)
    
        self.Append(CPPPATH = [self.BuildDir(inc_dir)])
        self.Append(LIBPATH = [self.BuildDir(lib_dir)])
    
    if self.SupportedPlatform('posix'):
        lib_name = dep[3:] if dep.startswith('lib') else dep
    else:
        lib_name = dep
    
    self.Append(LIBS = [lib_name])

def AddDeps(self, *deps):    
    print >> sys.stderr, GetSConscriptPath(self), ': AddDeps is deprecated, use UseSubsystems instead'
    for dir, dep in deps:    
        AddDependency(self, dir, dep)

def UseSubsystems(self, *subsystems):    
    if self['_INTERNAL']:
        ss_list_path = PathJoin(Dir('#').abspath, 'lib', 'subsystems.list')
    else:
        ss_list_path = PathJoin(env['TSLOAD_DEVEL_PATH'], 'lib', 'subsystems.list')
    
    self['SSLISTPATH'] = ss_list_path
    self['SUBSYSTEMS'] = subsystems
    
    ss_list = SubsystemList(subsystems)
    ss_list.read(ss_list_path)
    ss_list.build()
        
    # Get libraries needed for that target and add their dependencies 
    libs = [ss.library 
            for ss in ss_list 
            if ss.library is not None]    
    deps = []
    for lib in libs:
        if lib not in deps:
            AddDependency(self, 'lib', lib)
            deps.append(lib)

def InstallTarget(self, tgtroot, tgtdir, target):
    prefix = self['PREFIX']
    
    if GetOption('debian_packages'):
        prefix = PathJoin(prefix, debian_package_name(tgtroot))
    
    tgtdir = Dir(PathJoin(prefix, tgtdir))
    
    if 'SHLIBVERSION' in self and hasattr(self, 'InstallVersionedLib'):
        install = self.InstallVersionedLib(tgtdir, target)
    else:
        install = self.Install(tgtdir, target)
    
    self.Alias('install', install)
    self.Alias(tgtroot, install)
    
    return install

# Compile and build various targets
def CompileSharedLibrary(self, extra_sources = [], 
                         ctfconvert = True):
    objects = self.SharedObject(Glob('*.c') +                              \
                                Glob(PathJoin(env['PLATDIR'], '*/*.c')) +  \
                                extra_sources)
    
    # In Solaris we need to convert types from DWARF into CTF
    if self['DEBUG'] and ctfconvert and self.SupportedPlatform('solaris') and self['CTFCONVERT']:
        # ctfobjs = filter(lambda o: str(o.srcnode).endswith('.cpp'), ctfobjs)
        
        self.AddPostAction(objects, CommandAction('$CTFCONVERT -l $TSVERSION $TARGET'))
    
    return objects
    
def CompileProgram(self, extra_sources = []):
    usage = self.UsageBuilder('usage.txt')
    
    if self['SUBSYSTEMS']:
        # Programs need to init some subsystems global state
        subsys = self.SubsysBuilder()   
    
    objects = self.Object(Glob("*.c") +                                \
                          Glob(PathJoin(env['PLATDIR'], '*/*.c')) +    \
                          extra_sources)
    
    return objects

def LinkProgram(self, target, objects):
    program = self.Program(target, objects)
    
    # XXX: See below in LinkSharedLibrary
    env['TESTBINS'][target] = program
    
    return program

def LinkSharedLibrary(self, target, objects, 
                      ctfmerge = True, versioned = False):
    # Add x.y.z version to a soname
    if versioned and self.SupportedPlatform('posix'):
        tsload_version = self['TSVERSION']
        shlib_version = tsload_version[:tsload_version.index('-')]
        self['SHLIBVERSION'] = shlib_version
    
    library = self.SharedLibrary(target, objects)
    
    if self['DEBUG'] and ctfmerge and self.SupportedPlatform('solaris') and self['CTFMERGE']:
        self.AddPostAction(library, '$CTFMERGE -l $TSVERSION -o $TARGET $SOURCES')
    
    if self['LINK'] == 'link':
        # For Microsoft linker library contains .dll, .lib and .exp file
        # return only .dll file because only it needs to be installed
        library = filter(lambda f: str(f).endswith(env['SHLIBSUFFIX']), library)
    
    # XXX: LinkSharedLibrary called with context of cloned environment
    # We need to modify global env, so we call it. It's working :)
    # Another option is adding this line to each library SConscript 
    env['TESTLIBS'][target] = library
    
    return library

def Module(self, mod_type, mod_name, etrace_sources = []):   
    mod = self
    mod['SHLIBPREFIX'] = ''
    
    mod['_MODULE'] = True
    
    etrace_files = []
    man_files = []
    
    mod.UseSubsystems('tsload')
    modobjs = mod.CompileSharedLibrary()
    
    for src in etrace_sources:
        etrace_src_files, man_src_files = mod.PreprocessETrace(
                            [src] + modobjs, mod_name + mod['SHLIBSUFFIX'])
        etrace_files.extend(etrace_src_files)
        man_files.extend(man_src_files)
    
    module = mod.LinkSharedLibrary(mod_name, modobjs + etrace_files, 
                                   versioned = False)
    
    mod_install_dirs = { 'load': mod['INSTALL_MOD_LOAD'] }
    
    mod.InstallTarget('tsload-modules', mod_install_dirs[mod_type], module)    
    if man_files:
        mod.InstallTarget('tsload-modules', mod['INSTALL_SHARE'], man_files)
    
    # XXX: See above in LinkSharedLibrary
    env['TESTMODS'][mod_name] = module
    
    return module

def CheckBinary(self, name, paths = []):
    paths += self['ENV']['PATH'].split(os.pathsep)
    
    for path in paths:
        full_path = PathJoin(path, name)
        
        if PathIsFile(full_path) and PathAccess(full_path, os.X_OK):
            return full_path
    
    return None

def FindMicrosoftSDK(self):
    # Search for windows SDK. SCons doesn't support newest SDKs so implement our own
    winsdk_path = GetOption('with_windows_sdk')
    
    if not winsdk_path:
        sdk_locations = ["C:\\Program Files\\Microsoft SDKs\\Windows\\", 
                         "C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\"]
        sdk_versions = [ "v7.1", "v7.0A", "v7.0", "v6.1", "v6.0a", "v6.0" ]
        
        for location, version in product(sdk_locations, sdk_versions):
            winsdk_path = PathJoin(location, version)
            
            if PathExists(winsdk_path):
                break
        else:
            raise StopError("FindMicrosoftSDK: couldn't find Microsoft SDK in one of predefined paths, use --with-windows-sdk")
    
    self.Append(CPPPATH = [PathJoin(winsdk_path, 'Include')])
    if env['TARGET_ARCH'] == 'amd64' or env['TARGET_ARCH'] == 'x86_64':
        self.Append(LIBPATH = [PathJoin(winsdk_path, 'Lib\\x64')])
    else:
        self.Append(LIBPATH = [PathJoin(winsdk_path, 'Lib')])
    self.Append(LIBS = ['Kernel32.lib'])

def PostPrintCommandLine(s, target, source, env):    
    try:    
        # Print each target onto console
        if s.startswith('Copy('):
            action = 'COPY'
        else:
            action, _ = s.split(':')
        
        for tgtnode in target:
            if isinstance(tgtnode, Base):
                for abspath in [env.BuildDir(''), 
                                env.BuildDir('', subdir='test'), 
                                env['PREFIX']]:
                    try:
                        path = PathRemoveAbs(tgtnode.abspath, abspath)
                    except ValueError:
                        continue
                    else:
                        break
            else:
                path = tgtnode
            print ' %-12s %s' % (action, path) 
    except Exception:
        print s

def PrintCommandLine(self, action):
    ''' Hide build/install directory and pretty print action to console '''    
    if 'cmdline' in env['VERBOSE_BUILD']:
        return lambda target, source, env: '%s:$TARGET' % action

env.AddMethod(GetSConscriptPath)
env.AddMethod(AddDependency)
env.AddMethod(AddDeps)
env.AddMethod(UseSubsystems)
env.AddMethod(Macroses)
env.AddMethod(BuildDir)
env.AddMethod(Module)
env.AddMethod(InstallTarget)
env.AddMethod(SupportedPlatform)
env.AddMethod(CompileSharedLibrary)
env.AddMethod(CompileProgram)
env.AddMethod(LinkProgram)
env.AddMethod(LinkSharedLibrary)
env.AddMethod(FindMicrosoftSDK)
env.AddMethod(CheckBinary)
env.AddMethod(PrintCommandLine)

env.SetDefault(VERBOSE_BUILD = [])
env.SetDefault(_INTERNAL = False)
env.SetDefault(_MODULE = False)
env.SetDefault(PREFIX = None)

if not env['_INTERNAL']:
    # Read environment
    import json
    
    buildenv = file(PathJoin(env['TSLOAD_DEVEL_PATH'], 'scons.bldenv.json'))    
    benv = json.load(buildenv)
    
    env.Append(**benv)
    
    # guess TSLOADPATH (prefix)
    tgtdevel = PathJoin(env['INSTALL_SHARE'], 'devel')
    env['PREFIX'] = env['TSLOADPATH'] = PathRemove(env['TSLOAD_DEVEL_PATH'], tgtdevel)

# Add paths
if env['_INTERNAL']:
    env.Append(CPPPATH = [PathJoin(env['TSLOADPATH'], 'include'),   # TSLoad include directory (defs.h)
                          env.BuildDir('include'),                  # Generated includes (genbuild.h, genconfig.h) 
                          'include'])                               # Own subproject includes
    env.Append(LIBPATH = [])
else:
    env.Append(CPPPATH = [PathJoin(env['TSLOADPATH'], env['INSTALL_INCLUDE']), 
                          'include'])
    
    env.Append(LIBPATH = [PathJoin(env['TSLOADPATH'], env['INSTALL_LIB'])])

env.Append(LIBS = [])
env.Append(SUBSYSTEMS = [])
env.Append(WINRESOURCES = [])

env.Append(GENERATED_FILES = [])

# Usage Builder
UsageBuilder = Builder(action = Action('%s $TSLOADPATH/tools/genusage.py $SOURCE > $TARGET' % (sys.executable),
                                       env.PrintCommandLine('USAGE')),
                       src_suffix = '.txt',
                       suffix = '.c')
env.Append(BUILDERS = {'UsageBuilder': UsageBuilder})

# Subsystem Build
def subsys_emitter(target, source, env):
    if not target: 
        target = ['init.c']
    if not source:
        source = env['SSLISTPATH']
    return target, source
SubsysBuilder = Builder(action = Action('%s $TSLOADPATH/tools/gensubsys.py -c $SOURCE -f init $SUBSYSTEMS > $TARGET' % (sys.executable),
                                       env.PrintCommandLine('GENSUBSYS')),
                       emitter = subsys_emitter)
env.Append(BUILDERS = {'SubsysBuilder': SubsysBuilder})

# Prettify SCons output
if 'cmdline' not in env['VERBOSE_BUILD']:
    env.Append(CCCOMSTR     = 'CC:$TARGET',
               SHCCCOMSTR   = 'CC [SH]:$TARGET',
               LINKCOMSTR   = 'LD:$TARGET',
               ARCOMSTR     = 'LD:$TARGET',
               SHLINKCOMSTR = 'LD [SH]:$TARGET',
               INSTALLSTR   = 'INSTALL:$TARGET')
    env['PRINT_CMD_LINE_FUNC'] = PostPrintCommandLine

# This maps library/module/binary under test name to path to it
env.Append(TESTLIBS = {})
env.Append(TESTMODS = {})
env.Append(TESTBINS = {})

if env['_INTERNAL']:
    # Set default compilation parameters if we are building TSLoad from scratch
    
    # Add verbosity of warnings but disable unnecessary warnings
    if env['CC'] == 'gcc': 
        env.Append(CCFLAGS = ['-Wall'])
        env.Append(CCFLAGS = ['-Wno-unused-label', '-Wno-unused-variable', 
                              '-Wno-unused-function', '-Wno-switch', 
                              '-Werror-implicit-function-declaration'])
    elif env['CC'] == 'cl':
        env.Append(CCFLAGS = ['/W3'])
    
    mach = env['TARGET_ARCH']
    if mach:
        if env['CC'] == 'gcc': 
            if re.match('i\d86', mach) or mach == 'x86' or mach == 'sparc':
                env.Append(CCFLAGS = ["-m32"])
                env.Append(LINKFLAGS = ["-m32"])
                env['DTRACEOPTS'] = '-32'
            else:
                env.Append(CCFLAGS = ["-m64"])
                env.Append(LINKFLAGS = ["-m64"])
                env['DTRACEOPTS'] = '-64'
    
    # Determine build flags (debug/release)
    if env['DEBUG']:
        if env['CC'] == 'gcc':
            env.Append(CCFLAGS = ["-g"])
        elif env['CC'] == 'cl':
            # FIXME: Build service fails when creating PDBs on Windows
            # C1902 - looks like cl.exe couldn't connect to mspdbsrv.exe
            if 'TSLOAD_PDBSERV_DISABLE' not in os.environ:
                env.Append(CCFLAGS = ["/Z7"])
                env.Append(LINKFLAGS =  ["/debug"])
            env.Append(CCFLAGS = ['/Od'])           
    else:
        if env['CC'] == 'gcc':
            env.Append(CCFLAGS = ['-O2'])
        elif env['CC'] == 'cl':
            env.Append(CCFLAGS = ['/O2'])
    
Export('env')