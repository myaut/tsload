from pathutil import *

tgtroot = 'tsload-devel'

Import('env')

tgtdevel = PathJoin(env['INSTALL_SHARE'], 'devel')

def SaveEnvironmentImpl(target, source, env, vars, lists):
    import json
    try:
        from collections import OrderedDict
    except ImportError:
        # Order is only for prettiness, actually irrelevant
        # so fall back to dict, if Python is too old
        OrderedDict = dict
    
    # Convert environment list to Python list and filter out 'variables'
    to_list = lambda l: filter(lambda v: not v.startswith('$'), list(l)) 
    
    envdict = OrderedDict()
    for var in vars:
        envdict[var] = env[var]
    for lvar in lists:
        envdict[lvar] = to_list(env[lvar])
        
    saveenv = file(str(target[0]), 'w')
    json.dump(envdict, saveenv, indent = 4)
    saveenv.close()
    
def SaveEnvironmentSCons(target, source, env):
    vars = ['DEBUG', 'TARGET_ARCH', 'TSVERSION', 'ETRACEENABLED', 
            'PLATCHAIN', 'PLATDEBUG', 'PLATCACHE', 'PLATDIR',
            'INSTALL_BIN', 'INSTALL_ETC', 'INSTALL_INCLUDE', 'INSTALL_LIB',
            'INSTALL_MOD_LOAD', 'INSTALL_SHARE', 'INSTALL_VAR']
    lists = ['CCFLAGS', 'LINKFLAGS']    
    
    SaveEnvironmentImpl(target, source, env, vars, lists)
    
def SaveEnvironmentMake(target, source, env):
    vars = ['SHLIBSUFFIX', 'SHOBJSUFFIX']
    lists = ['CCFLAGS', 'SHCCFLAGS', 'LIBS', 'SHLINKFLAGS', 'LINKFLAGS']   
    
    SaveEnvironmentImpl(target, source, env, vars, lists)
    
def SaveEnvironment(env, bldenv, command):    
    # Generate build environment    
    benv = env.Clone()
    benv.UseSubsystems('tsload')
    saveenv = File(benv.BuildDir(bldenv)) 
    benv.Command(saveenv, [], Action(command, benv.PrintCommandLine('SAVEENV')))
    benv.Alias('install', saveenv)
    benv.InstallTarget(tgtroot, tgtdevel, saveenv)

SaveEnvironment(env, 'scons.bldenv.json', SaveEnvironmentSCons)
SaveEnvironment(env, 'make.bldenv.json', SaveEnvironmentMake)

# Install includes and build helpers
build_helpers = [ # SCons build helpers
                 'SConscript.env.py', 'SConscript.plat.py', 
                 'SConscript.install.py', 'SConscript.etrace.py',
                 'SConscript.ext.py', 'lib/subsystems.list',
                 'tools/build/pathutil.py', 'tools/build/subsys.py',
                 'tools/build/installdirs.py', 
                  # Generator builders
                 'tools/gensubsys.py', 'tools/genetrace.py', 'tools/genusage.py',
                  # PLATAPI
                 'tools/plat/plat.py', 'tools/plat/portalocker.py', 
                 'tools/plat/parse-include.py', 'tools/plat/proc-source.py']

for entry in Dir('#include').glob('*'):
    env.InstallTarget(tgtroot, env['INSTALL_INCLUDE'], entry)

dest_dir = PathJoin(env['INSTALL_INCLUDE'], 'tsload')
env.InstallTarget('tsload-devel', dest_dir, env['GENERATED_FILES'])

for helper in build_helpers: 
    env.InstallTarget(tgtroot, PathJoin(tgtdevel, PathDirName(helper)), helper)  

Export('env')