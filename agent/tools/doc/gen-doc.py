import os
import sys

from tsdoc.parser import SourceParser
from tsdoc.markdown import MarkdownPrinter

_ = sys.argv.pop(0)
header_path = sys.argv.pop(0)

header = SourceParser(header_path)
header.parse()

for source_path in sys.argv: 
    source = SourceParser(source_path)
    source.parse()
    header.merge(source)
 
header.do_print(MarkdownPrinter(sys.stdout))