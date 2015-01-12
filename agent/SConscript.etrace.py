import sys
import os

from pathutil import *

Import('env')

if 'etrace' in env['VERBOSE_BUILD']:
    env.Append(ENV = {'ETRACEDEBUG': True})

if env.SupportedPlatform('win') and env['ETRACEENABLED']:
    EtraceBuilder = Builder(action = Action('%s $TSLOADPATH/tools/genetrace.py -m ETW $SOURCE $ETRACEEXEFILE > $TARGET' % (sys.executable),
                                            env.PrintCommandLine('ETRACE')),
                            suffix = '.man')
    EtraceRCBuilder = Builder(action = Action('mc -r $ETRACEDIR -h $ETRACEDIR $SOURCE', env.PrintCommandLine('ETRACE [MC]')), 
                                              suffix = '.rc', src_suffix = '.man')
    EtraceRESBuilder = Builder(action = Action('rc /fo $TARGET $SOURCE', env.PrintCommandLine('ETRACE [RC]')), 
                                               suffix = '.res', src_suffix = '.rc')
    env.Append(BUILDERS = {'EtraceBuilder': EtraceBuilder,
                           'EtraceRCBuilder': EtraceRCBuilder,
                           'EtraceRESBuilder': EtraceRESBuilder})
    env['ETRACESUFFIX'] = '.man'
elif env.SupportedPlatform('linux') or env.SupportedPlatform('solaris') and env['ETRACEENABLED']:
    # Normally DTrace uses .d suffixes, but modern versions of support 
    # D language (from Digital Mars), so it confuses SCons
    env['ETRACESUFFIX'] = '.dd'
    
    EtraceBuilder = Builder(action = Action('%s $TSLOADPATH/tools/genetrace.py -m USDT $SOURCE $ETRACEEXEFILE > $TARGET' % (sys.executable),
                                            env.PrintCommandLine('ETRACE')),
                            suffix = env['ETRACESUFFIX'])
    env.Append(BUILDERS = {'EtraceBuilder': EtraceBuilder})
    
    if env.SupportedPlatform('solaris'):
        DTraceBuilder = Builder(action = Action('dtrace -xnolibs -G $DTRACEOPTS -o $TARGET -s $SOURCES',
                                                env.PrintCommandLine('DTRACE')),
                                src_suffix = env['ETRACESUFFIX'],
                                suffix = '.o')
        env.Append(BUILDERS = {'DTraceBuilder': DTraceBuilder})   
    
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
