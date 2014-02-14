import os

from SCons.Node.Alias import Alias

from pathutil import *
from installdirs import install_dirs

Import('env')

def Stringify(s):
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    return '"%s"' % s

AddOption('--prefix',  dest='prefix', action="store", default = '',
          metavar='PREFIX', help='Installation prefix') 

# Define installation prefixes
env['PREFIX'] = GetOption('prefix')

if not env['PREFIX']:
    env['PREFIX'] = PathJoin(env['TSLOADPATH'], 'build', env['TSNAME'])

if not PathIsAbs(env['PREFIX']):
    env['PREFIX'] = PathAbsolute(env['PREFIX'])

gen_inc_dir = Dir(env.BuildDir('include'))
gen_install = gen_inc_dir.File('geninstall.h')

conf_install = Configure(env, config_h = str(gen_install))

for key, dir, win_dir, param, help in install_dirs:
    default = win_dir if env.SupportedPlatform('win') else dir    
    
    AddOption('--' + param,  dest=param, action="store", default=default,
              metavar='DIR', help=help + ' [EPREFIX/%default]')
    env[key] = GetOption(param)
    
    conf_install.Define(key, Stringify(env[key]), comment = help)

conf_install.Finish()

def ZipArchive(target, source, env):
    # Since default Zip target in SCons will add variant_dir, 
    # we need to redefine arcname in zf.write 
    import zipfile
    
    compression = env.get('ZIPCOMPRESSION', 0)
    
    zip_node = target[0]
    if isinstance(zip_node, str):
        zip_node = File(zip_node) 
    
    zf = zipfile.ZipFile(str(zip_node), 'w', compression)
    for s in source:
        if isinstance(s, Alias):
            continue
        
        if s.isdir():
            for dirpath, dirnames, filenames in PathWalk(str(s)):
                for fname in filenames:
                    path = PathJoin(dirpath, fname)
                    if PathIsFile(path):
                        arcpath = PathJoin(env['TSNAME'], PathRelative(path, env['PREFIX']))
                        zf.write(path, arcpath)
        else:
            path = str(s)
            arcpath = PathJoin(env['TSNAME'], PathRelative(path, env['PREFIX']))
            zf.write(path, arcpath)
    zf.close()

ZipArchiveBuilder = Builder(action = ZipArchive, suffix = '.zip')

env.Append(BUILDERS = {'ZipArchive': ZipArchiveBuilder})

Export('env')