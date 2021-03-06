# In Windows we install all binaries into installation root and prettify directories names
# env[key]    POSIX directory    WIN directory     param name    param help
install_dirs = [('INSTALL_BIN',      'bin/',          '',   
                    'bindir',   'binary files'),
                ('INSTALL_LIB',      'lib/',          '',    
                    'libdir',   'shared libraries'),
                ('INSTALL_ETC',      'etc/',             'Configuration\\',   
                    'etcdir',   'configuration files'),
                ('INSTALL_INCLUDE',      'include/',             'Include\\',   
                    'includedir',   'include files (headers)'),
                ('INSTALL_VAR',      'var/tsload/',       'Data\\',   
                    'vardir',   'variable files'),
                ('INSTALL_SHARE',    'share/tsload/',      'Shared\\',   
                    'sharedir',   'shared files'),
                ('INSTALL_MAN',    'share/man/',      'ManualPages\\',   
                    'mandir',   'manual pages'),
                ('INSTALL_MOD_LOAD', 'lib/tsload/mod-load/',     'LoadModules\\',   
                    'loadmoddir',   'loader modules')]
                
def debian_package_name(pkg_name):
    # Convert tsload package to Debian package name
    pkg_map = {'tsload-devel': 'tsload-dev',
               'tsload-doc':   'tsload-doc'}
    return pkg_map.get(pkg_name, 'tsload')