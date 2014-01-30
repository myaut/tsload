from pathutil import *
from buildstr import GenerateBuildFile
from SCons.Errors import StopError
import SCons.SConf

Import('env')

if 'configure' in BUILD_TARGETS:
    SCons.SConf.SetCacheMode('force')

gen_inc_dir = Dir(env.BuildDir('include'))
gen_config = gen_inc_dir.File('genconfig.h')
gen_build = gen_inc_dir.File('genbuild.h')

conf = Configure(env, config_h = str(gen_config))

# Make python string C string literal
def Stringify(s):
    return '"%s"' % s

def CheckBinary(context, name, paths = []):
    paths += context.env['ENV']['PATH'].split(os.pathsep)
    
    context.Message('Checking for platform has %s program...' % name)
    
    for path in paths:
        full_path = PathJoin(path, name)
        
        if PathIsFile(full_path) and PathAccess(full_path, os.X_OK):
            context.sconf.Define(name.upper() + '_PATH', Stringify(full_path),
                           'Defined to path to binary if it exists on build system')
            context.Result(True)
            return full_path
    
    context.Result(False)

def CheckDesignatedInitializers(context):
    test_source = """
    struct S {
        int a;
        int b;
    };
    
    int main() {
        struct S s = { .b = -1, .a = 100 };
    }
    """
    
    context.Message('Checking for C compiler supports designated inilializers...')
    ret = context.sconf.TryCompile(test_source, '.c')
    
    if ret:
        context.sconf.Define('HAVE_DESIGNATED_INITIALIZERS', 
                       comment='Compiler supports designated inilializers')
    
    context.Result(ret)

def CheckGCCSyncBuiltins(context):
    test_source = """
    int main() {
        long value;
         __sync_fetch_and_add(&value, 187);
    }
    """
    
    context.Message('Checking if GCC supports __sync builtins...')
    ret = context.sconf.TryCompile(test_source, '.c')
    
    if ret:
        context.sconf.Define('HAVE_SYNC_BUILTINS', 
                       comment='GCC compiler supports sync builtins')
    
    context.Result(ret)
    
    return ret

def CheckGCCAtomicBuiltins(context):
    test_source = """
    int main() {
        long value;
         __atomic_fetch_add(&value, 187, __ATOMIC_ACQ_REL);
    }
    """
    
    context.Message('Checking if GCC supports __atomic builtins...')
    ret = context.sconf.TryCompile(test_source, '.c')
    
    if ret:
        context.sconf.Define('HAVE_ATOMIC_BUILTINS', 
                       comment='GCC compiler supports atomic builtins')
    
    context.Result(ret)
    
    return ret

conf.AddTests({'CheckBinary': CheckBinary,
               'CheckDesignatedInitializers': CheckDesignatedInitializers,
               'CheckGCCSyncBuiltins': CheckGCCSyncBuiltins,
               'CheckGCCAtomicBuiltins': CheckGCCAtomicBuiltins})

#-------------------------------------------
# C compiler and standard library checks

if not conf.CheckHeader('stdarg.h'):
    raise StopError('stdarg.h was not found!')

# long long is used widely across agent code
if not conf.CheckType('long long'):
    raise StopError("Your compiler doesn't support 'long long', abort!")

# int64_t is used by ts_time_t
if not conf.CheckType('int64_t', '#include <stdint.h>\n'):
    raise StopError("Your compiler doesn't support 'int64_t', abort!")

conf.CheckDesignatedInitializers()

conf.CheckDeclaration('min', '#include <stdlib.h>')
conf.CheckDeclaration('max', '#include <stdlib.h>')

if not conf.CheckDeclaration('snprintf', '#include <stdio.h>'):
    if not conf.CheckDeclaration('_snprintf', '#include <stdio.h>'):
        raise StopError('snprintf or _snprintf are not supported by your platform!')
    
    conf.Define('snprintf', '_snprintf', 'Redefine sprintf')
    
if env['CC'] == 'gcc':
    if not conf.CheckGCCAtomicBuiltins() and not conf.CheckGCCSyncBuiltins():
        raise StopError("GCC doesn't support neither atomic nor __sync builtins")

# ------------------------------------------
# Global platform checks    

if env.SupportedPlatform('posix'):
    if not conf.CheckHeader('unistd.h'):
        raise StopError('unistd.h is missing')
    
    if not conf.CheckLib('pthread'):
        raise StopError("POSIX thread library was not found!")

if env.SupportedPlatform('win'):
    if not conf.CheckHeader('windows.h'):
        raise StopError('windows.h was not found!')
    
conf.CheckType('boolean_t', '#include <sys/types.h>\n')

#----------------------------
# tscommon checks

if GetOption('debug'):
    conf.Define('TS_LOCK_DEBUG', comment='--debug was enabled')

# Get system id of thread
if env.SupportedPlatform('linux'):
    conf.CheckDeclaration('__NR_gettid', '#include <sys/syscall.h>')

if env.SupportedPlatform('solaris'):
    conf.CheckDeclaration('_lwp_self', '#include <sys/lwp.h>')

#----------------------------
# hostinfo checks

if env.SupportedPlatform('linux'):
    lsb_release = conf.CheckBinary('lsb_release')

# ----------------------------
# tstime checks

if env.SupportedPlatform('posix'):
    if not conf.CheckDeclaration('nanosleep', '#include <time.h>'):
        raise StopError("Your platform doesn't support nanosleep()")

    if not conf.CheckDeclaration('gettimeofday', '#include <sys/time.h>'):
        raise StopError("Your platform doesn't support gettimeofday()")

if env.SupportedPlatform('linux'):
    if not conf.CheckLib('rt'):
        raise StopError("librt is missing")
    
    if not conf.CheckDeclaration('clock_gettime', '#include <time.h>'):
        raise StopError("clock_gettime() is missing")

if env.SupportedPlatform('solaris'):
    if not conf.CheckLib('rt'):
        raise StopError("librt is missing")
        
# ----------------------------
# log checks
if env.SupportedPlatform('linux'):
    conf.CheckHeader('execinfo.h')

# ----------------------------
# mempool checks
if not GetOption('mempool_alloc'):
    conf.Define('MEMPOOL_USE_LIBC_HEAP', comment='--enable-mempool-alloc was specified')

conf.CheckHeader('valgrind/valgrind.h')
if GetOption('mempool_valgrind'):
    conf.Define('MEMPOOL_VALGRIND', comment='--enable-valgrind was specified')
    
if GetOption('trace'):
    conf.Define('MEMPOOL_TRACE', comment='--trace was enabled')

# ----------------------------
# etrace checks
if env.SupportedPlatform('win'):
    if GetOption('etw') and conf.CheckDeclaration('EventRegister', '''#include <windows.h>
                                                                      #include <evntprov.h>'''):
        conf.Define('ETRC_USE_ETW', comment='--enable-etw was enabled and supports manifest-based events')

if env.SupportedPlatform('solaris') or env.SupportedPlatform('linux'):
    if GetOption('usdt') and conf.CheckHeader('sys/sdt.h'):
        conf.Define('ETRC_USE_USDT', comment='--enable-usdt was enabled')
    
# ----------------------------
# client checks
if env.SupportedPlatform('posix'):
    if not conf.CheckDeclaration('poll', '#include <poll.h>'):
        raise StopError('poll() is missing')
    
    if not conf.CheckDeclaration('inet_aton', '''#include <arpa/inet.h>'''):
        raise StopError('inet_aton() is missing')
    
if env.SupportedPlatform('win'):
    if not conf.CheckDeclaration('WSAStartup', '#include <winsock2.h>'):
        raise StopError('WinSock are not implemented')
        
# ----------------------------
# UUID checks
if env.SupportedPlatform('linux') or env.SupportedPlatform('solaris'):
    if not conf.CheckDeclaration('uuid_generate_time', '#include <uuid/uuid.h>'):
        raise StopError('uuid_generate_time() is missing. Maybe you need libuuid-devel or something?')

#------------------------------
# hostinfo checks?
if GetOption('trace'):
    conf.Define('HOSTINFO_TRACE', comment='--trace was enabled')

# -----------------------------
# ctfconvert / ctfmerge (Solaris)

if env.SupportedPlatform('solaris'):
    ONBLD_PATH = [ # Solaris 11 path
                  '/opt/onbld/bin/i386',
                  '/opt/onbld/bin/sparc',
                  
                  # Solaris 10 path
                  '/opt/SUNWonbld/bin/i386',
                  '/opt/SUNWonbld/bin/sparc']
    
    env['CTFCONVERT'] = conf.CheckBinary('ctfconvert', ONBLD_PATH)
    env['CTFMERGE'] = conf.CheckBinary('ctfmerge', ONBLD_PATH)

env.Alias('configure', gen_config)

env = conf.Finish()

#------------------------------------
# Generate build strings
if GetOption('update_build'):
    env.AlwaysBuild(env.Command(gen_build, [], GenerateBuildFile))
