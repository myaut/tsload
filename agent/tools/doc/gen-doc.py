import os
import sys

from collections import defaultdict

from tsdoc import TSDoc
from tsdoc.page import TSDocPage, MarkdownPage, IndexPage

from tsdoc.blocks import Link
from tsdoc.blocks.markdown import MarkdownPrinter
from tsdoc.blocks.html import HTMLPrinter

# Main code

_ = sys.argv.pop(0)
index_path = sys.argv.pop(0) 

# Destination dir
doc_dir = os.path.dirname(index_path)

# Output format
doc_format = os.getenv('TSDOC_FORMAT', 'markdown')
doc_header = os.getenv('TSDOC_HEADER', '')

if doc_format == 'html':
    doc_suffix = '.html'
    printer = HTMLPrinter(os.getenv('TSDOC_HTML_TEMPLATE'))
elif doc_format == 'markdown':
    doc_suffix = '.md'
    printer = MarkdownPrinter()
else:
    raise ValueError("Invalid documentation format '%s'" % doc_format)

pages = defaultdict(dict)
tsdoc_pages = []

print 'Parsing ', 

for page_path in sys.argv:
    page_name = os.path.basename(page_path)
    print page_name, 
    if page_path.endswith('.src.md'):
        page = MarkdownPage(page_path)
    elif page_path.endswith('.tsdoc'):
        page = TSDocPage(page_path)
        tsdoc_pages.append(page)
    
    pages[page.docspace][page.name] = page

print '\nProcessing',

for page in tsdoc_pages:
    print '%s/%s' % (page.docspace, page.name),
    page.process()

print '\nBuilding index...'

# Build indexes and Cross-References
index_page = IndexPage(index_path, doc_header, pages)
index_page.generate(printer, doc_dir, doc_suffix)