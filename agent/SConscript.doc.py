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
else:
    raise StopError("Invalid documentation format '%s'" % doc_format)

env.Append(ENV = {'TSDOC_FORMAT': doc_format})

DocBuilder = Builder(action = '%s $TSLOADPATH/tools/doc/build-doc.py $SOURCES > $TARGET' % (sys.executable),
                     suffix = '.tsdoc')
DocGenerator = Builder(action = '%s $TSLOADPATH/tools/doc/gen-doc.py $SOURCE $TSDOC' % (sys.executable))

env.Append(BUILDERS = {'DocBuilder': DocBuilder,
                       'DocGenerator': DocGenerator})
env.Append(TSDOC = [])

Export('env')