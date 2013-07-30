import sys
import os

PathJoin = os.path.join
PathBaseName = os.path.basename
PathExists = os.path.exists

Import('env')

def Macroses(self, *macroses):
    prefix = '/D' if self['CC'] == 'cl' else '-D'
        
    for macro in macroses:
        self.Append(CCFLAGS = [prefix + macro])
    
def SupportedPlatform(self, platform):
    return platform in self['PLATCHAIN']

def BuildDir(self, dir):
    return PathJoin(self['TSLOADPATH'], 'build', 'build', dir)
 
def AddDeps(self, *deps):
    for dir, dep in deps:
        inc_dir = PathJoin(dir, dep, 'include')
        lib_dir = PathJoin(dir, dep)
        
        self['CPPPATH'] += [self.BuildDir(inc_dir)]
        self['LIBPATH'] += [self.BuildDir(lib_dir)]
        self['LIBS'] += [dep]

def InstallTarget(self, tgtdir, target):
    if 'install' in BUILD_TARGETS or 'zip' in BUILD_TARGETS:
        tgtdir = Dir(PathJoin(env['PREFIX'], tgtdir))
        install = self.Install(tgtdir, target)
        self.Alias('install', install)
        
        return install[0]

# Compile and build various targets
def CompileSharedLibrary(self, extra_sources = [], 
                         ctfconvert = True):
    objects = self.SharedObject(Glob("src/*.c") + Glob("plat/*/*.c") + extra_sources)
    
    # In Solaris we need to convert types from DWARF into CTF
    if ctfconvert and self.SupportedPlatform('solaris') and self['CTFCONVERT']:
        # ctfobjs = filter(lambda o: str(o.srcnode).endswith('.cpp'), ctfobjs)
        
        self.AddPostAction(objects, CommandAction('$CTFCONVERT -l $TSVERSION $TARGET'))
    
    return objects
    
def CompileProgram(self, extra_sources = []):
    objects = self.Object(Glob("src/*.c") + Glob("plat/*/*.c") + extra_sources)
    return objects

def LinkSharedLibrary(self, target, objects, 
                      ctfmerge = True):
    library = self.SharedLibrary(target, objects)
    
    if ctfmerge and self.SupportedPlatform('solaris') and self['CTFMERGE']:
        self.AddPostAction(library, '$CTFMERGE -l $TSVERSION -o $TARGET $SOURCES')
    
    if env['LINK'] == 'link':
        # For Microsoft linker library contains .dll, .lib and .exp file
        # return only .dll file because only it needs to be installed
        library = filter(lambda f: str(f).endswith(env['SHLIBSUFFIX']), library)
    
    # XXX: LinkSharedLibrary called with context of cloned environment
    # We need to modify global env, so we call it. It's working :)
    # Another option is adding this line to each library SConscript 
    env['TESTLIBS'][target] = library
    
    return library

def Module(self, mod_type, mod_name):   
    mod = self
    mod.Macroses('NO_JSON')
    mod['SHLIBPREFIX'] = ''
    
    mod.AddDeps(('lib', 'libtscommon'),
                ('lib', 'libhostinfo'), 
                ('lib', 'libtsload'))
    modobjs = mod.CompileSharedLibrary()
    module = mod.LinkSharedLibrary(mod_name, modobjs)
    
    mod_install_dirs = { 'load': mod['INSTALL_MOD_LOAD'] }
    mod.InstallTarget(mod_install_dirs[mod_type], module)
    
    return module

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
    if env['TARGET_ARCH'] == 'amd64':
        self.Append(LIBPATH = [PathJoin(winsdk_path, 'Lib\\x64')])
    else:
        self.Append(LIBPATH = [PathJoin(winsdk_path, 'Lib')])
    self.Append(LIBS = ['Kernel32.lib'])

env.AddMethod(AddDeps)
env.AddMethod(Macroses)
env.AddMethod(BuildDir)
env.AddMethod(Module)
env.AddMethod(InstallTarget)
env.AddMethod(SupportedPlatform)
env.AddMethod(CompileSharedLibrary)
env.AddMethod(CompileProgram)
env.AddMethod(LinkSharedLibrary)
env.AddMethod(FindMicrosoftSDK)

env.Append(CPPPATH = [PathJoin(env['TSLOADPATH'], 'include'),   # TSLoad include directory (defs.h)
                      env.BuildDir('include'),                  # Generated includes (genbuild.h, genconfig.h) 
                      'include'])                               # Own subproject includes  
env.Append(LIBPATH = [])
env.Append(LIBS = [])

# TESTLIBS maps library under test name to path to it
env.Append(TESTLIBS = {})

# Add verbosity of warnings
if env['CC'] == 'gcc': 
    env.Append(CCFLAGS = ['-Wall'])
    env.Append(CCFLAGS = ['-Wno-unused-label', '-Wno-unused-variable', '-Wno-unused-function'])
elif env['CC'] == 'cl':
    env.Append(CCFLAGS = ['/W3'])

# Determine build flags (debug/release)
if env['DEBUG']:
    if env['CC'] == 'gcc':
        env.Append(CCFLAGS = ["-g"])
    elif env['CC'] == 'cl':
        env.Append(CCFLAGS = ["/Zi", '/Od'])
        env.Append(LINKFLAGS =  ["/debug"])
    
else:
    if env['CC'] == 'gcc':
        env.Append(CCFLAGS = ['-O2'])
    elif env['CC'] == 'cl':
        env.Append(CCFLAGS = ['/O2'])
    
Export('env')