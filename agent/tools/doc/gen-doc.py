import os
import sys

from tsdoc.parser import SourceParser
from tsdoc.markdown import MarkdownPrinter
from tsdoc.html import HTMLPrinter

doc_formats = {'markdown': MarkdownPrinter,
               'html': HTMLPrinter}
printer_class = doc_formats[os.getenv('TSDOC_FORMAT', 'markdown')]

_ = sys.argv.pop(0)
header_path = sys.argv.pop(0)

header = SourceParser(header_path)
header.parse()

for source_path in sys.argv: 
    source = SourceParser(source_path)
    source.parse()
    header.merge(source)

printer = printer_class(sys.stdout)
header.do_print(printer)