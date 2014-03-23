import sys
import os

from pathutil import *

Import('env')

env.Append(ENV = {'ETRACEDEBUG': True})

if env.SupportedPlatform('win') and env['ETRACEENABLED']:
    EtraceBuilder = Builder(action = '%s $TSLOADPATH/tools/genetrace.py -m ETW $SOURCE $ETRACEEXEFILE > $TARGET' % (sys.executable),
                            suffix = '.man')
    EtraceRCBuilder = Builder(action = 'mc -r $ETRACEDIR -h $ETRACEDIR $SOURCE', suffix = '.rc', src_suffix = '.man')
    EtraceRESBuilder = Builder(action = 'rc /fo $TARGET $SOURCE', suffix = '.res', src_suffix = '.rc')
    env.Append(BUILDERS = {'EtraceBuilder': EtraceBuilder,
                           'EtraceRCBuilder': EtraceRCBuilder,
                           'EtraceRESBuilder': EtraceRESBuilder})
    env['ETRACESUFFIX'] = '.man'
elif env.SupportedPlatform('linux') or env.SupportedPlatform('solaris') and env['ETRACEENABLED']:
    EtraceBuilder = Builder(action = '%s $TSLOADPATH/tools/genetrace.py -m USDT $SOURCE $ETRACEEXEFILE > $TARGET' % (sys.executable),
                            suffix = '.d')
    env.Append(BUILDERS = {'EtraceBuilder': EtraceBuilder})
    
    if env.SupportedPlatform('solaris'):
        DTraceBuilder = Builder(action = 'dtrace -xnolibs -G $DTRACEOPTS -o $TARGET -s $SOURCES',
                                suffix = '.o')
        env.Append(BUILDERS = {'DTraceBuilder': DTraceBuilder})
    
    env['ETRACESUFFIX'] = '.d'
else:
    env['ETRACEENABLED'] = False    

def PreprocessETrace(self, sources, target):
    etrace_files = []
    man_files = []
    
    if self['ETRACEENABLED']:
        src_filename = sources[0]
        src_name, _ = src_filename.rsplit('.', 1)
        
        obj_file = [obj
                    for obj in sources[1:]
                    if str(obj).startswith(src_name)]     
        
        man_filename = src_name + self['ETRACESUFFIX']
        
        # XXX: Does all SConscripts Clone() their environments?
        self['ETRACEEXEFILE'] = PathJoin(self['PREFIX'], target)
        self['ETRACEDIR'] = PathJoin(Dir('.').path, PathDirName(src_filename))
        
        man = self.EtraceBuilder(man_filename, src_filename)
        
        if self.SupportedPlatform('win'):
            rc_file = self.EtraceRCBuilder(src_name + '.rc', man)
            res_file = self.EtraceRESBuilder(src_name + '.res', rc_file)
            
            etrace_files.append(res_file)
        
        if self.SupportedPlatform('solaris'):
            dtrace_file = self.DTraceBuilder(src_name + '_provider' + self['SHOBJSUFFIX'],
                                             [man, obj_file])
            dtrace_file[0].attributes.shared = True
            
            etrace_files.append(dtrace_file[0])
        
        man_files.append(man)
            
    return etrace_files, man_files

env.AddMethod(PreprocessETrace)

Export('env')
