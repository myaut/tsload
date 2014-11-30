import sys
from pathutil import *

from SCons.Errors import StopError 

tgtroot = 'tsload-doc'

Import('env')

doc_format = GetOption('doc_format')

if doc_format == 'html':
    doc_suffix = '.html'
elif doc_format == 'markdown':
    doc_suffix = '.md'
elif doc_format == 'latex':
    doc_suffix = '.tex'
elif doc_format == 'creole':
    doc_suffix = '.creole'            
else:
    raise StopError("Invalid documentation format '%s'" % doc_format)

def modify_doc_targets(target, source, env):
    def variant_tgt(entry):    
        # header files are located in the global directory (include/)
        # however, emitter will get an absolute path
        # so, make it again relative and put targets into build dir        
        name = str(Dir('#').rel_path(entry))    
        name = env.BuildDir(PathJoin('tsdoc', name))    
        
        # XXX: SCons do not deletes old targets from in-memory fs 
        # causing glob() to emit them. Do it manually!
        entries = entry.get_dir().entries
        basename = PathBaseName(name)
        if basename in entries:        
            del entries[basename]
        
        return env.fs.File(name)
    
    return map(variant_tgt, target), source

env.Append(ENV = {'TSDOC_FORMAT': doc_format})

DocBuilder = Builder(action = '%s $TSLOADPATH/tools/doc/build-doc.py $TSDOC_SPACE $SOURCES > $TARGET' % (sys.executable),
                     suffix = '.tsdoc',
                     emitter = modify_doc_targets)
DocGenerator = Builder(action = '%s $TSLOADPATH/tools/doc/gen-doc.py $SOURCE $TSDOC' % (sys.executable))

env.Append(BUILDERS = {'DocBuilder': DocBuilder,
                       'DocGenerator': DocGenerator})
env.Append(TSDOC = [])

Export('env')