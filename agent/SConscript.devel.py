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
    lists = ['CCFLAGS']    
    
    SaveEnvironmentImpl(target, source, env, vars, lists)
    
def SaveEnvironmentMake(target, source, env):
    vars = ['SHLIBSUFFIX', 'SHOBJSUFFIX']
    lists = ['CCFLAGS', 'SHCCFLAGS', 'LIBS', 'SHLINKFLAGS']   
    
    SaveEnvironmentImpl(target, source, env, vars, lists)
    
def SaveEnvironment(env, bldenv, command):    
    # Generate build environment    
    benv = env.Clone()
    benv.AddDeps(('lib', 'libtscommon'),
                 ('lib', 'libhostinfo'), 
                 ('lib', 'libtsjson'),
                 ('lib', 'libtsobj'),
                 ('lib', 'libtsload'))
    saveenv = File(benv.BuildDir(bldenv)) 
    benv.Command(saveenv, [], command)
    benv.Alias('install', saveenv)
    benv.InstallTarget(tgtroot, tgtdevel, saveenv)

SaveEnvironment(env, 'scons.bldenv.json', SaveEnvironmentSCons)
SaveEnvironment(env, 'make.bldenv.json', SaveEnvironmentMake)

# Install includes and build helpers
build_helpers = ['SConscript.env.py', 'SConscript.plat.py', 
                 'SConscript.install.py', 'SConscript.etrace.py',
                 'SConscript.ext.py', 'tools/build', 'tools/plat']

for entry in Dir('#include').glob('*'):
    env.InstallTarget(tgtroot, env['INSTALL_INCLUDE'], entry)

dest_dir = PathJoin(env['INSTALL_INCLUDE'], 'tsload')
env.InstallTarget('tsload-devel', dest_dir, env['GENERATED_FILES'])

for helper in build_helpers:
    env.InstallTarget(tgtroot, tgtdevel, helper)  

Export('env')