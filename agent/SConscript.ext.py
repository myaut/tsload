from SCons.Action import ActionFactory
from SCons.Script.SConscript import SConsEnvironment

import os
import sys

PathJoin = os.path.join

Import('env')

SConsEnvironment.Chmod = ActionFactory(os.chmod,
        lambda dest, mode: 'Chmod("%s", 0%o)' % (dest, mode)) 

# Append path to build tools
sys.path.append(PathJoin(env['TSLOAD_DEVEL_PATH'], 'build'))

# Load tsload SConscripts
SConscript(PathJoin(env['TSLOAD_DEVEL_PATH'], 'SConscript.env.py'), 'env')
SConscript(PathJoin(env['TSLOAD_DEVEL_PATH'], 'SConscript.plat.py'), 'env')
SConscript(PathJoin(env['TSLOAD_DEVEL_PATH'], 'SConscript.install.py'), 'env')
SConscript(PathJoin(env['TSLOAD_DEVEL_PATH'], 'SConscript.etrace.py'), 'env')

Export('env')