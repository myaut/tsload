import os
import sys

import json

from tsdoc.parser import SourceParser

_ = sys.argv.pop(0)
docspace = sys.argv.pop(0)
header_path = sys.argv.pop(0)

header = SourceParser(header_path)
header.parse()

for source_path in sys.argv: 
    source = SourceParser(source_path)
    source.parse()
    header.merge(source)
    
tsdoc = header.create_tsdoc()
tsdoc.set_docspace(docspace)
tsdoc.find_header()

obj = tsdoc.serialize()
json.dump(obj, sys.stdout, indent = 2)
