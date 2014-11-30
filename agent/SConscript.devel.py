from pathutil import *

tgtroot = 'tsload-devel'

Import('env')

tgtdevel = PathJoin(env['INSTALL_SHARE'], 'devel')

def SaveEnvironment(target, source, env):
    import json
    try:
        from collections import OrderedDict
    except ImportError:
        # Order is only for prettiness, actually irrelevant
        # so fall back to dict, if Python is too old
        OrderedDict = dict
        
    vars = ['DEBUG', 'TARGET_ARCH', 'TSVERSION', 'ETRACEENABLED', 
            'PLATCHAIN', 'PLATDEBUG', 'PLATCACHE', 'PLATDIR',
            'INSTALL_BIN', 'INSTALL_ETC', 'INSTALL_INCLUDE', 'INSTALL_LIB',
            'INSTALL_MOD_LOAD', 'INSTALL_SHARE', 'INSTALL_VAR']
    
    envdict = OrderedDict()
    envdict['CCFLAGS'] = list(env['CCFLAGS'])    
    for var in vars:
        envdict[var] = env[var]
        
    saveenv = file(str(target[0]), 'w')
    json.dump(envdict, saveenv, indent = 4)
    saveenv.close()

# Generate build environment    
saveenv = File(env.BuildDir('buildenv.json')) 
env.Command(saveenv, [], SaveEnvironment)
env.Alias('install', saveenv)
env.InstallTarget(tgtroot, tgtdevel, saveenv)

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