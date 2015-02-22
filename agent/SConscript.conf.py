from pathutil import *
from buildstr import GenerateBuildFile
from SCons.Errors import StopError
import SCons.SConf
import sys

Import('env')

if 'configure' in BUILD_TARGETS:
    SCons.SConf.SetCacheMode('force')

gen_inc_dir = Dir(env.BuildDir(PathJoin('include', 'tsload')))
gen_config = gen_inc_dir.File('genconfig.h')
gen_build = gen_inc_dir.File('genbuild.h')

conf = Configure(env, config_h = str(gen_config))

# Make python string C string literal
def Stringify(s):
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    return '"%s"' % s

def CheckBinary(context, name, paths = []):
    context.Message('Checking for platform has %s program...' % name)
    full_path = env.CheckBinary(name, paths)
    
    if full_path is not None:
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

def CheckUnalignedMemAccess(context):
    test_source = """
    #include <stddef.h>
    #include <stdint.h>
    #include <stdlib.h>
    
    int main() {
            char* test = malloc(12);
            volatile uint64_t val = *((uint64_t*) (test+4));
    
            return 0;
    }
    """
    
    context.Message('Checking if processor allows unaligned accesses...')
    ret, _ = context.sconf.TryRun(test_source, '.c')
    
    if ret:
        context.sconf.Define('HAVE_UNALIGNED_MEMACCESS', 
                       comment='Processor allows unaligned accesses')
    
    context.Result(ret)
    
    return ret

def CheckDladmOpen(context):
    test_source = """
    #include <libdladm.h>
    
    int main() {
        dladm_handle_t h;
        dladm_open(&h, "test");
    }
    """
    
    context.Message('Checking if dladm_open() has second argument...')
    ret = context.sconf.TryCompile(test_source, '.c')
    
    if ret:
        context.sconf.Define('HAVE_DLADM_V2', 
                       comment='dladm_open() have second argument')
    
    context.Result(ret)
    
    return ret

def CheckVsprintfSupportsCounting(context, func, args):
    test_source = """
    #include <stdio.h>
    #include <stdarg.h>
    
    int test(const char* fmt, ...) {
        int ret = 0;
        
        va_list va;
        va_start(va, fmt);
        ret = """ + func + "(" + args + """ fmt, va);
        va_end(va);
        
        return ret;
    }
    
    int main() {    
        if(test("test %d is %s", 42, "abyrvalg") == 19)
            return 0;
        return 1;
    }
    """
    
    context.Message('Checking if ' + func + '() accepts NULL as buffer...')
    ret, _ = context.sconf.TryRun(test_source, '.c')
    
    if ret > 0:
        context.sconf.Define('HAVE_COUNTING_' + func.upper(), 
                       comment= func + '() accepts NULL as buffer')
    
    context.Result(ret)
    
    return ret

conf.AddTests({'CheckBinary': CheckBinary,
               'CheckDesignatedInitializers': CheckDesignatedInitializers,
               'CheckGCCSyncBuiltins': CheckGCCSyncBuiltins,
               'CheckGCCAtomicBuiltins': CheckGCCAtomicBuiltins,
               'CheckUnalignedMemAccess': CheckUnalignedMemAccess,
               'CheckDladmOpen': CheckDladmOpen,
               'CheckVsprintfSupportsCounting': CheckVsprintfSupportsCounting})

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
conf.CheckDeclaration('round', '#include <math.h>')

if not conf.CheckDeclaration('snprintf', '#include <stdio.h>'):
    if not conf.CheckDeclaration('_snprintf', '#include <stdio.h>'):
        raise StopError('snprintf or _snprintf are not supported by your platform!')
    
    conf.Define('snprintf', '_snprintf', 'Redefine sprintf')

if not conf.CheckDeclaration('strtoll', '#include <stdlib.h>'):
    if not conf.CheckDeclaration('_strtoi64', '#include <stdlib.h>'):
        raise StopError('strtoll or _strtoi64 are not supported by your platform!')
    
    conf.Define('strtoll', '_strtoi64', 'Redefine strtoll')

    
if env['CC'] == 'gcc':
    if not conf.CheckGCCAtomicBuiltins() and not conf.CheckGCCSyncBuiltins():
        raise StopError("GCC doesn't support neither atomic nor __sync builtins")

conf.CheckHeader('inttypes.h')
conf.CheckUnalignedMemAccess()

conf.CheckDeclaration('va_copy', '#include <stdarg.h>')

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

if GetOption('trace'):
    conf.Define('TS_LOCK_DEBUG', comment='--trace was enabled')

# Get system id of thread
if env.SupportedPlatform('linux'):
    conf.CheckDeclaration('__NR_gettid', '#include <sys/syscall.h>')

if env.SupportedPlatform('solaris'):
    conf.CheckDeclaration('_lwp_self', '#include <sys/lwp.h>')
    conf.CheckDeclaration('pset_bind_lwp', '#include <sys/pset.h>')
    
    conf.CheckDeclaration('smbios_open', '#include <sys/smbios.h>')

#----------------------------
# aas_vprintf() checks 
if not conf.CheckVsprintfSupportsCounting('_vscprintf', '') and               \
        not conf.CheckVsprintfSupportsCounting('vsnprintf', 'NULL, 0, ') and  \
		not conf.CheckVsprintfSupportsCounting('vsprintf', 'NULL, ') 	      :
    raise StopError("No way to evaluate length of string in aas_vprintf()")

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
    
    if not conf.CheckDeclaration('inet_ntop', '''#include <arpa/inet.h>'''):
        raise StopError('inet_ntop() is missing')
    
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

if env.SupportedPlatform('solaris'):
    conf.CheckDladmOpen()

#------------------------------
# Module checks
# HTTP:
if conf.CheckDeclaration('curl_easy_perform', '#include <curl/curl.h>') and conf.CheckLib('curl'):
    env['HAVE_CURL'] = True
else:
    env['HAVE_CURL'] = False
    print >> sys.stderr, 'WARNING: libcurl missing, building of `http` module would be disabled'

#==============================

env.Alias('configure', gen_config)

env = conf.Finish()

#------------------------------------
# Generate build strings
env.Command(gen_build, [], Action(GenerateBuildFile, 
                                  env.PrintCommandLine('GENBUILD')))
if GetOption('update_build'):
    env.AlwaysBuild(gen_build)

env.Append(GENERATED_FILES = [gen_build, gen_config])

Export('env')