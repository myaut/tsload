import sys
import re

from pathutil import *

from collections import defaultdict

from SCons.Errors import StopError 
from SCons.Node import NodeList

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

env['TSDOC_DOCSPACES'] = defaultdict(list)

def modify_doc_targets(target, source, env):
    docspaces = env['TSDOC_DOCSPACES']
    
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
        
        # Save corresponding docspace for emit_doc_targets()
        docspace = env['TSDOC_SPACE']
        docname = basename[:-6]
        docspaces[docspace].append(docname)
        
        return env.fs.File(name)
    
    return map(variant_tgt, target), source

def emit_doc_targets(target, source, env):
    # FIXME: this doesn't work well with single-filed docgenerators (like latex)
    index = source[0]
    
    # First of all, read index file, but instead of parsing it, use
    # simple regular expressions to process docspaces and extra files
    idxf = file(index.srcnode().abspath)
    docspaces = env['TSDOC_DOCSPACES']
    
    for line in idxf:
        m = re.search('\[__docspace__:(\w*)\]', line)
        if m:
            docspace = m.group(1)
                        
            if '[__external_index__]' in line:
                docspaces[docspace].append('index')
            if '[__reference__]' in line:
                docspaces[docspace].append('reference')
    
    # Now process document targets that go from TSDOC env var
    for nodelist in env['TSDOC']:
        if not isinstance(nodelist, NodeList):
            # This is a single-node emitted from AppendDoc in doc/SConsctript
            nodelist = [nodelist]
        
        for node in nodelist:
            path = node.get_path()
            fname = PathBaseName(path)
            
            if fname.endswith('.src.md'):
                # This is traditional doc - use its dir as docspace and 
                # file name as document name (remove extension of course)
                docspace = PathBaseName(PathDirName(path))
                docname = fname[:-7]
                
                docspaces[docspace].append(docname)
            # .tsdocs was handled earlier in modify_doc_targets
    
    # Time to emit some files
    target = []
    
    def add_target(docname, docspace = None):
        path = docname + doc_suffix
        if docspace:
            path = PathJoin(docspace, path)
                
        tgt = File(path)
        target.append(tgt)
    
    for docspace in docspaces:
        for docname in docspaces[docspace]:
            add_target(docname, docspace)
    
    idxpath = index.get_path()
    
    assert idxpath.endswith('.src.md')
    add_target(idxpath[:-7])
    
    return target, source

env.Append(ENV = {'TSDOC_FORMAT': doc_format})
if 'tsdoc' in env['VERBOSE_BUILD']:
    env.Append(ENV = {'TSDOC_VERBOSE': True})

DocBuilder = Builder(action = Action('%s $TSLOADPATH/tools/doc/build-doc.py $TSDOC_SPACE $SOURCES > $TARGET' % (sys.executable),
                                     env.PrintCommandLine('DOCBUILD')),
                     suffix = '.tsdoc',
                     emitter = modify_doc_targets)
DocGenerator = Builder(action = Action('%s $TSLOADPATH/tools/doc/gen-doc.py $SOURCE $TSDOC' % (sys.executable),
                                       env.PrintCommandLine('DOCGEN')),
                       emitter = emit_doc_targets)
ManGenerator = Builder(action = Action('%s $TSLOADPATH/tools/doc/gen-man-page.py $SOURCE $TARGET' % (sys.executable),
                                       env.PrintCommandLine('MANPAGE')),
                       src_suffix = '.src.md')

env.Append(BUILDERS = {'DocBuilder': DocBuilder,
                       'DocGenerator': DocGenerator,
                       'ManGenerator': ManGenerator})
env.Append(TSDOC = [])

Export('env')