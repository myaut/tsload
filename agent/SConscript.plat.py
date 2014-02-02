import sys
import os

PathJoin = os.path.join
PathBaseName = os.path.basename
PathExists = os.path.exists

Import('env')

# Prepares platform-specific build
# 1. Copies includes from plat/<platform>/*.h to plat/*.h
# 2. Creates tasks for source platform preprocessing
def PreparePlatform(self):
    self.Append(ENV = {'PLATCACHE': PathJoin(Dir('.').path, self['PLATCACHE'])})
    self.Append(ENV = {'PLATDEBUG': self['PLATDEBUG']})
    
    plat_chain = self['PLATCHAIN']
    rev_plat_chain = list(reversed(plat_chain))
    
    plat_cache = self['PLATCACHE']
    plat_includes = []
    
    self.PlatIncBuilder(plat_cache, Dir('include').glob('*.h'))
    
    # Build platform-chained includes and sources
    
    for plat_name in plat_chain:      
        # For each platform-dependent include select best match and copy
        # it from plat/<platform>/include.h to plat/include.h
        for inc_file in Dir('include').Dir('plat').Dir(plat_name).glob('*.h'): 
            base_name = PathBaseName(str(inc_file))
            dest_file = PathJoin('include/plat', base_name)
            
            if base_name not in plat_includes:
                self.Command(dest_file, inc_file, Copy("$TARGET", "$SOURCE"))
                
                plat_includes.append(base_name)
    
    for plat_name in rev_plat_chain:
        # Parse source files for platform-api function implementation
        # and select best match for it
        for src_file in Dir('src').Dir('plat').Dir(plat_name).glob('*.c'):            
            tgt_file = File(str(src_file).replace(PathJoin('src', 'plat'), 'plat'))            
            
            # Each source file depends on platform cache
            # and all source that are more specific than this file
            # Otherwise SCons will select generic implementation
            base_name = PathBaseName(str(src_file))
            
            # We need it for dependent platforms, because VariantDir was not processed yet
            src_path = Dir('src').srcnode().abspath
            
            self.Depends(tgt_file, plat_cache)
            
            # Add dependencies for all available platforms to ensure that
            # most specific platform will be built earlier than "generic" platform
            for dep_plat_name in rev_plat_chain[rev_plat_chain.index(plat_name) + 1:]:
                dep_plat_path = PathJoin('plat', dep_plat_name, base_name)
                
                # Cannot use SCons.Node here because it creates dependencies for non-existent files
                dep_file = PathJoin(src_path, dep_plat_path)
                if not PathExists(dep_file):
                    continue
                
                self.Depends(tgt_file, File(dep_plat_path))
            
            # Add builder 
            self.PlatSrcBuilder(tgt_file, src_file)

env.AddMethod(PreparePlatform)

PLATFORM_CHAIN = {'win32': ['win', 'generic'],
                  'linux2': ['linux', 'posix', 'generic'],
                  'sunos5': ['solaris', 'posix', 'generic']
                  }

try:
    env['PLATCHAIN'] = PLATFORM_CHAIN[sys.platform]
except KeyError:
    raise StopError('Unsupported platform "%s"' % sys.platform)

env.Macroses(*('PLAT_' + plat.upper() for plat in env['PLATCHAIN']))

env['PLATDEBUG'] = True
env['PLATCACHE'] = 'plat_cache.pch'

PlatIncBuilder = Builder(action = '%s tools/plat/parse-include.py $SOURCES' % (sys.executable),
                         src_suffix = '.h')
PlatSrcBuilder = Builder(action = '%s tools/plat/proc-source.py $SOURCE > $TARGET' % (sys.executable),
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
    env.Append(LIBS = ['c'])
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
    
Export('env')