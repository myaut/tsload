'''
TSDoc blocks

intermediate doc representation before gerating HTML/Markdown/etc.

Two main classes:
    - Block - block of text, contains other blocks, and text in array
    - Text - represents any text (for plain text use __str__)
By default renders into plain text via __str__, use tsdoc.printer, to
create formatted text.

For example:

Block(Header(1, "Recomendation"]),
      Paragraph(['Use', Link('Google', 'http://google.com/'),
                 'to search for', BoldText('Information')]),
      Code('int a = 777;'))

is matches with this text

# Recomendation

Use [Google](http://google.com/) to search for **Information**

```
int a = 777;
```

'''

import sys

class Text(object):
    def __init__(self, text):
        self.text = text
    
    def __str__(self):
        return self.text
    
    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, 
                           repr(self.text))

class BoldText(Text):
    pass

class ItalicText(Text):
    pass

class InlineCode(Text):
    pass

class Reference(Text):
    pass

class CodeReference(Reference):
    def __init__(self, text, ref_name, ref_class):
        Reference.__init__(self, text)
        
        self.ref_name = ref_name
        self.ref_class = ref_class

class Label(Text):
    def __init__(self, text, style):
        Text.__init__(self, text)
        self.style = style
        
    def __repr__(self):
        return 'Label(%s, %s)' % (repr(self.style), 
                                  repr(self.text)) 

class Header(Text):
    def __init__(self, text, size):
        Text.__init__(self, text)
        self.size = size
        
    def __repr__(self):
        return 'Header(%d, %s)' % (self.size,
                                   repr(self.text))

class Link(Text):
    INTERNAL = 0
    EXTERNAL = 1
    INVALID = 2
    
    def __init__(self, text, type, where):
        Text.__init__(self, text)
        self.type = type
        self.where = where
        
    def __repr__(self):
        return 'Link(%d, %s, %s)' % (self.type, repr(self.where), 
                                     repr(self.text))

class Image(Link):
    pass

class Block(object):
    def __init__(self, parts = []):
        if parts:
            self.parts = parts
        else:
            self.parts = []
    
    def add(self, text):
        self.parts.append(text)
        
    def extend(self, text_list):
        self.parts.extend(text_list)
        
    def __iter__(self):
        return iter(self.parts)
    
class ListEntry(Block):
    def __init__(self, level, parts = []):
        self.level = level
        Block.__init__(self, parts)
    
    def __repr__(self):
        return 'ListEntry(%d, %s)' % (self.level, 
                                      self.parts)

class ListBlock(Block):
    pass

class Paragraph(Block):
    pass

class Code(Paragraph):
    pass

class NavLink(object):
    PREV = 0
    NEXT = 1
    UP   = 2
    HOME = 3
    REF  = 4
    
    def __init__(self, type, page, where):
        self.type = type
        self.page = page
        self.where = where

def pprint_block(block, stream = sys.stdout, indent = 0):
    def do_print(s):
        print >> stream, ' ' * indent, s,
        
    prefix = '%s(\n' % block.__class__.__name__
    do_print(prefix)
    
    indent += len(prefix)
    
    for part in block:
        if isinstance(part, Block):
            pprint_block(part, stream, indent)
        else:
            do_print(repr(part) + ',\n')
    
    do_print(')\n')
        