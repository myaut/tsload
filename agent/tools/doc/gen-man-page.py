import sys
import os
import time

from tsdoc.page import MarkdownPage
from tsdoc.blocks import *
from tsdoc.blocks.groff import GroffPrinter

def format_title(mdfile, manfile):
    # Use modification date of original mdfile in manpage
    st = os.stat(mdfile)
    page_date = time.strftime('%B %Y', time.gmtime(st.st_mtime))
    
    cmd_name, section = manfile.rsplit('.', 1)
    cmd_name = os.path.basename(cmd_name)
    
    header = os.getenv('TSDOC_HEADER')
    
    return '.TH {0} {1} "{2}" "{3}"'.format(cmd_name, section, page_date, header)

mdfile, manfile = sys.argv[1:]

manprinter = GroffPrinter()
page = MarkdownPage(mdfile)

if isinstance(page.blocks[0].parts[0], Header):
    # Remove first block which is header of entire page because we already 
    # mentioned command name in the title
    page.blocks.pop(0)

# Add some generic information to man page
page.blocks.append(
    Paragraph([
        Header('SEE ALSO', 4),
        Text('For more information, please refer to TSLoad documentation '
            'which may already be installed locally and which is also available '
            'online at ' + os.getenv('TSDOC_MAN_URL') + '.')])
    )
              
page.blocks.append(
    Paragraph([Header('AUTHOR', 4),
               Text(os.getenv('TSDOC_MAN_AUTHOR'))])
    )

with open(manfile, 'w') as man:
    # Will need tbl preprocessor (Solaris man needs to specify it explicitly)
    print >> man, """'\\" t"""
    
    print >> man, format_title(mdfile, manfile)
    manprinter.do_print(man, '', page)