import sys
import os

PathJoin = os.path.join
PathBaseName = os.path.basename
PathExists = os.path.exists

Import('env')

# Prepares platform-specific build
# 1. Copies includes from plat/<platform>/*.h to plat/*.h
# 2. Creates tasks for source platform preprocessing
def PreparePlatform(self, inc_dir):
    self.Append(ENV = {'PLATCACHE': PathJoin(Dir('.').path, self['PLATCACHE'])})
    if self['PLATDEBUG']:
        self.Append(ENV = {'PLATDEBUG': self['PLATDEBUG']})
    
    plat_chain = self['PLATCHAIN']
    rev_plat_chain = list(reversed(plat_chain))
    
    plat_cache = self['PLATCACHE']
    plat_includes = []
    
    # Includes are located in several directories:
    #    BUILD/include/.../plat     - build_inc_dir - for platform-specific headers    
    #    #include/.../              - root_inc_dir  - for headers with PLATAPI (public)
    #    include/                   -               - for headers with PLATAPI (private)
    # inc_dir contains include/ as first part of path, replace it with real include prefix
    inc_dir = os.path.normcase(inc_dir)
    inc_dir_parts = inc_dir.split(os.sep)
    # ...
    build_inc_dir = self.BuildDir(inc_dir)
    dest_dir = PathJoin(env['INSTALL_INCLUDE'], *(inc_dir_parts[1:] + ['plat']))    
    inc_dir = '#' + inc_dir
    
    self.PlatIncBuilder(plat_cache, Dir(inc_dir).glob('*.h') + Dir('include').glob('*.h'))
    
    # Build platform-chained includes and sources
    
    for plat_name in plat_chain:      
        # For each platform-dependent include select best match and copy
        # it from plat/<platform>/include.h to <inc_dir>/plat/include.h
        for inc_file in Dir('plat').Dir(plat_name).glob('*.h'): 
            base_name = PathBaseName(str(inc_file))
            dest_file = PathJoin(build_inc_dir, 'plat', base_name)
            
            if base_name not in plat_includes:                
                self.Command(dest_file, inc_file, Copy("$TARGET", "$SOURCE"))
                
                # Install plat include into tsload-devel
                if not env['_MODULE']:
                    env.InstallTarget('tsload-devel', dest_dir, dest_file)
                
                plat_includes.append(base_name)
    
    for plat_name in rev_plat_chain:
        # Parse source files for platform-api function implementation
        # and select best match for it. Destination is `curplat'
        for src_file in Dir('plat').Dir(plat_name).glob('*.c'):            
            tgt_file = File(str(src_file).replace('plat', self['PLATDIR']))            
            
            # Each source file depends on platform cache
            # and all source that are more specific than this file
            # Otherwise SCons will select generic implementation
            base_name = PathBaseName(str(src_file))
            
            # We need it for dependent platforms, because VariantDir was not processed yet
            src_path = Dir('.').srcnode().abspath
            
            self.Depends(tgt_file, plat_cache)
            
            # Add dependencies for all available platforms to ensure that
            # most specific platform will be built earlier than "generic" platform
            for dep_plat_name in rev_plat_chain[rev_plat_chain.index(plat_name) + 1:]:
                dep_plat_path = PathJoin(dep_plat_name, base_name)
                
                # Cannot use SCons.Node here because it creates dependencies for non-existent files
                dep_file = PathJoin(src_path, 'plat', dep_plat_path)
                if not PathExists(dep_file):
                    continue
                
                self.Depends(tgt_file, File(PathJoin(self['PLATDIR'], dep_plat_path)))
            
            # Add builder 
            self.PlatSrcBuilder(tgt_file, src_file)

env.AddMethod(PreparePlatform)

if env['_INTERNAL']:
    PLATFORM_CHAIN = {'win32': ['win', 'generic'],
                      'linux2': ['linux', 'posix', 'generic'],
                      'sunos5': ['solaris', 'posix', 'generic']
                      }
    
    try:
        env['PLATCHAIN'] = PLATFORM_CHAIN[sys.platform]
    except KeyError:
        raise StopError('Unsupported platform "%s"' % sys.platform)
    
    env.Macroses(*('PLAT_' + plat.upper() for plat in env['PLATCHAIN']))
    
    env['PLATDEBUG'] = 'plat' in env['VERBOSE_BUILD']
    env['PLATCACHE'] = 'plat_cache.pch'
    env['PLATDIR'] = 'curplat'

PlatIncBuilder = Builder(action = Action('%s tools/plat/parse-include.py $SOURCES' % (sys.executable), 
                                         env.PrintCommandLine('PLATINC')),
                         src_suffix = '.h')
PlatSrcBuilder = Builder(action = Action('%s tools/plat/proc-source.py $SOURCE > $TARGET' % (sys.executable),
                                         env.PrintCommandLine('PLATSRC')),
                         suffix = '.c',
                         src_suffix = '.c')
env.Append(BUILDERS = {'PlatIncBuilder': PlatIncBuilder, 
                       'PlatSrcBuilder': PlatSrcBuilder})

# Setup C RTL
if env.SupportedPlatform('win'):
    if GetOption('debug'):
        env.Append(CCFLAGS = ['/MDd'])
    else:
        env.Append(CCFLAGS = ['/MD'])
    env.FindMicrosoftSDK()
elif env.SupportedPlatform('posix'):
    env.Append(LIBS = ['c', 'm'])
    env.Macroses('_GNU_SOURCE')
    env.Macroses('_REENTRANT')
    
# -----------------------------
# ctfconvert / ctfmerge (Solaris)

if env.SupportedPlatform('solaris'):
    ONBLD_PATH = [ # Solaris 11 path
                  '/opt/onbld/bin/i386',
                  '/opt/onbld/bin/sparc',
                  
                  # Solaris 10 path
                  '/opt/SUNWonbld/bin/i386',
                  '/opt/SUNWonbld/bin/sparc']
    
    env['CTFCONVERT'] = env.CheckBinary('ctfconvert', ONBLD_PATH)
    env['CTFMERGE'] = env.CheckBinary('ctfmerge', ONBLD_PATH)
    
    # See https://github.com/mapnik/mapnik/issues/675
    env.Append(SHLINKFLAGS = ['-shared'])
    
Export('env')