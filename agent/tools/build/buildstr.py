import sys
import time
import getpass
import socket
from subprocess import Popen, PIPE

import SCons

def Define(buildfile, param, value):
    print >> buildfile, '#define %-32s "%s"' % (param, value)

def GetBuildUser():
    try:
        return getpass.getuser()
    except:
        return 'unknown'
    
def GetBuildHost():
    return socket.gethostname()

def GetCompilerVersion(env):
    if env['CC'] == 'gcc':
        default = 'GNU C Compiler'
        args = ['--version']
    elif env['CC'] == 'cl':
        default = 'Microsoft C/C++ Compiler'
        args = []
    else:
        return ''
    
    pipe = SCons.Action._subproc(env, [env['CC']] + args,
                                 stdin = 'devnull',
                                 stderr = PIPE,
                                 stdout = PIPE)
    
    if pipe.wait() != 0: 
        return default
    
    if env['CC'] == 'gcc':
        version = pipe.stdout.readline()
    elif env['CC'] == 'cl':
        version = pipe.stderr.readline()
    
    return version.strip()

def GetBuildDateTime():
    return time.ctime()

def GetBuildYear():
    return time.gmtime().tm_year

def GetBuildCommandLine():
    return " ".join(sys.argv[1:])

def GenerateBuildFile(target, source, env):
    buildfile = file(str(target[0]), 'w')
    
    print >> buildfile, '#ifndef GEN_BUILD_H'
    print >> buildfile, '#define GEN_BUILD_H'
    
    Define(buildfile, 'BUILD_PROJECT', env['TSPROJECT'])
    Define(buildfile, 'BUILD_VERSION', env['TSVERSION'])
    Define(buildfile, 'BUILD_PLATFORM', sys.platform)
    Define(buildfile, 'BUILD_MACH', env['TARGET_ARCH'])
    Define(buildfile, 'BUILD_AUTHOR', env['TSAUTHOR'])
    
    Define(buildfile, 'BUILD_USER', GetBuildUser())
    Define(buildfile, 'BUILD_HOST', GetBuildHost())
    Define(buildfile, 'BUILD_DATETIME', GetBuildDateTime())
    Define(buildfile, 'BUILD_YEAR', GetBuildYear())
    
    Define(buildfile, 'BUILD_CMDLINE', GetBuildCommandLine())
    Define(buildfile, 'BUILD_COMPILER', GetCompilerVersion(env))
    
    print >> buildfile, '#endif'
    
    buildfile.close()