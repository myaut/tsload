import os
import sys
import string

from collections import defaultdict

from tsdoc import DEF, TypeDef, DefinitionGroup

MODULE_HEADER_SIZE  = 2
CLASS_HEADER_SIZE = 3
GROUP_HEADER_SIZE = 4

defclasses = [(DEF.DOCTEXT, ''),
              (DEF.PROTO, 'Functions'),
              (DEF.TYPEDEF, 'Types')]

PARAM_HEADER = 'PARAMETERS'
RETVALUE_HEADER = 'RETURN VALUE'
NOTES_HEADER = 'NOTES'

class SourceParser:
    def __init__(self, path):
        self.path = path
        
        # Transform 'include/threads.h' into 'threads' 
        filename = os.path.basename(path)
        self.module = filename[:filename.rindex('.')] 
        
        self.source = file(path, 'r')
        
        self.groups = defaultdict(list)
        self.defs = []
        
    def parse(self):
        lineno = 1
        group = DefinitionGroup()
        lastline = True
        
        for raw_line in self.source:
            empty = all(c in string.whitespace for c in raw_line)
            if lastline and empty:
                self.defs.extend(group.defs)
                defclass = group.get_defclass()
                self.groups[defclass].append(group)                
                group = DefinitionGroup()
            else:
                lastline = group.parse_line(lineno, raw_line)
                
            lineno += 1
        
        self._publish_types()
    
    def _publish_types(self):
        type_refs = []
        
        for defobj in self.defs:
            if defobj.public:
                type_refs.extend(defobj.type_refs)
        
        type_refs = set(type_refs)
        
        for defobj in self.defs:
            if isinstance(defobj, TypeDef):
                type_set = set([defobj.name] + defobj.aliases)
                if type_set & type_refs:
                    defobj.public = True                    
    
    def merge(self, other):
        doctext = other.groups[DEF.DOCTEXT]
        self.groups[DEF.DOCTEXT].extend(doctext)
        
        for group in other.groups[DEF.PROTO]:
            new_groups = []
            names = group.get_names(public=False)
            
            for my_group in self.groups[DEF.PROTO]:
                new_group = my_group.merge(group, names)
                
                if new_group is not None:
                    new_groups.append(new_group)
                
            if new_groups:
                self.groups[DEF.PROTO].extend(new_groups)
           
    def do_print(self, printer):
        printer.header(MODULE_HEADER_SIZE, self.module) 
        
        for (defclass, classheader) in defclasses:
            if defclass != DEF.DOCTEXT:    
                printer.header(CLASS_HEADER_SIZE, classheader)
                 
            for group in self.groups[defclass]:            
                if not group.is_public():
                    continue
                
                if defclass != DEF.DOCTEXT:
                    header = group.create_header()
                    printer.header(GROUP_HEADER_SIZE, header)
                
                for defobj in group.defs:    
                    if defobj.deftype in [DEF.COMMENT, DEF.DOCTEXT]:
                        self.print_comment(printer, defobj.get_text())
                    else:
                        printer.code_block(defobj.get_text())
    
    def print_comment(self, printer, comment):
        lines = comment.split('\n')
        line = ''
        
        headers = []
        def _print_header_once(header):
            if header not in headers:
                printer.italic_text(header)
                printer.new_line(2)
                headers.append(header)
        
        for vline in lines:
            line += vline
            if vline.endswith('\\'):
                line = line[:-1]
                continue
            
            if line.startswith('@'):
                directive, line = line.split(None, 1)
                if directive == '@param':
                    _print_header_once(PARAM_HEADER)   
                    
                    name, description = line.split(None, 1)                 
                    printer.list_entry()
                    printer.bold_text(name)
                    
                    printer.raw_text(' - ' + description)
                elif directive == '@return':
                    _print_header_once(RETVALUE_HEADER)
                    printer.raw_text(line)
                elif directive == '@note':
                    _print_header_once(NOTES_HEADER)
                    printer.raw_text(line)
            else:
                printer.raw_text(line)
            
            printer.new_line()
            line = ''
                
                
            
            
            