import os

PathJoin = os.path.join
PathIsAbs = os.path.isabs
PathAbsolute = os.path.abspath 

Import('env')

AddOption('--prefix',  dest='prefix', action="store", default = '',
          metavar='PREFIX', help='Installation prefix') 

# Define installation prefixes
env['PREFIX'] = GetOption('prefix')

if not env['PREFIX']:
    env['PREFIX'] = PathJoin(env['TSLOADPATH'], 'build', env['TSNAME'])

if not PathIsAbs(env['PREFIX']):
    env['PREFIX'] = PathAbsolute(env['PREFIX'])

env['ZIPFILE'] = PathJoin('#build', env['TSNAME']) + '$ZIPSUFFIX'

# In Windows we install all binaries into installation root and prettify directories names

# env[key]    POSIX directory    WIN directory     param name    param help
install_dirs = [('INSTALL_BIN',      'bin',          '',   
                    'bindir',   'loader modules'),
                ('INSTALL_LIB',      'lib',          '',    
                    'libdir',   'shared libraries'),
                ('INSTALL_ETC',      'etc',          'Configuration',   
                    'etcdir',   'configuration files'),
                ('INSTALL_VAR',      'var',          'Data',   
                    'vardir',   'variable files'),
                ('INSTALL_SHARE',    'share/tsload', 'Shared',   
                    'sharedir',   'shared files'),
                ('INSTALL_MOD_LOAD', 'mod/load',     'LoadModules',   
                    'loadmoddir',   'loader modules')]


for key, dir, win_dir, param, help in install_dirs:
    default = win_dir if env.SupportedPlatform('win') else dir    
    
    AddOption('--' + param,  dest=param, action="store", default=default,
              metavar='DIR', help=help + ' [EPREFIX/%default]')
    env[key] = GetOption(param)

Export('env')