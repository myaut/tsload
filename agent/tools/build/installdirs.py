# In Windows we install all binaries into installation root and prettify directories names
# env[key]    POSIX directory    WIN directory     param name    param help
install_dirs = [('INSTALL_BIN',      'bin/',          '',   
                    'bindir',   'loader modules'),
                ('INSTALL_LIB',      'lib/',          '',    
                    'libdir',   'shared libraries'),
                ('INSTALL_ETC',      'etc/',             'Configuration\\',   
                    'etcdir',   'configuration files'),
                ('INSTALL_VAR',      'var/tsload/',       'Data\\',   
                    'vardir',   'variable files'),
                ('INSTALL_SHARE',    'share/tsload/',      'Shared\\',   
                    'sharedir',   'shared files'),
                ('INSTALL_MOD_LOAD', 'lib/tsload/mod-load/',     'LoadModules\\',   
                    'loadmoddir',   'loader modules')]