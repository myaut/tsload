import os
import sys
import string

from collections import defaultdict

from tsdoc import DEF, TypeDef, DefinitionGroup

MODULE_HEADER_SIZE  = 1
CLASS_HEADER_SIZE = 2
GROUP_HEADER_SIZE = 3

defclasses = [(DEF.DOCTEXT, ''),
              (DEF.PROTO, 'Functions'),
              (DEF.TYPEDEF, 'Types'),
              (DEF.VARIABLE, 'Variables and constants'),]

PARAM_HEADER = 'PARAMETERS'
RETVALUE_HEADER = 'RETURN VALUE'
NOTES_HEADER = 'NOTES'
VALUE_HEADER = 'VALUES'
MEMBER_HEADER = 'MEMBERS'

class SourceParser:
    def __init__(self, path):
        self.is_header = path.endswith('.h')        
        self.path = path
        
        # Transform 'include/threads.h' into 'threads' 
        filename = os.path.basename(path)
        self.module = filename[:filename.rindex('.')] 
        self.default_module = True
        
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
                self._add_group(group)  
                group = DefinitionGroup()
            else:
                lastline = group.parse_line(lineno, raw_line)
                
            lineno += 1
        
        self._publish_types()
        self._find_module()
    
    def _add_group(self, group):
        group.update_tags(self.is_header)
        
        self.defs.extend(group.defs)
        defclass = group.get_defclass()
        
        self.groups[defclass].append(group)
          
    def _publish_types(self):
        ''' Find type references made by functions and 
        set public flag to types so types being documented too '''
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
    
    def _find_module(self):
        ''' Find @module directive and change header '''
        for defobj in self.defs:
            if defobj.deftype == DEF.DOCTEXT:
                module = defobj.find_directive('@module')
                
                if module:                    
                    self.module = module
                    self.default_module = False
                    
                    break
    
    def merge(self, other):
        if self.default_module and not other.default_module:
            self.module = other.module
            self.default_module = False
        
        doctext = other.groups[DEF.DOCTEXT]
        self.groups[DEF.DOCTEXT].extend(doctext)
        
        defclass = DEF.PROTO
        
        merged_groups = []
        
        for group in other.groups[defclass]:
            new_groups = []
            names = group.get_names(public=False)
            
            for my_group in self.groups[defclass][:]:
                new_group = my_group.merge(group, names)
                
                if new_group is not None:
                    self._add_group(new_group)
                    my_group.disabled = True
                    
                    merged_groups.append(group)
        
        for group in other.groups[DEF.PROTO] + other.groups[DEF.VARIABLE]:
            if group in merged_groups:
                continue
            
            if group.is_public():
                defclass = group.get_defclass()
                self.groups[defclass].append(group)
                
           
    def do_print(self, printer):
        printer.prepare(self.module)
        printer.header(MODULE_HEADER_SIZE, self.module) 
        
        in_code_block = False
        code = ''
        
        for (defclass, classheader) in defclasses:
            if all(group.disabled or not group.is_public()
                   for group in self.groups[defclass]):
                continue
            
            if defclass != DEF.DOCTEXT:
                printer.header(CLASS_HEADER_SIZE, classheader)
                 
            for group in self.groups[defclass]:            
                if group.disabled or not group.is_public():
                    continue
                
                if defclass != DEF.DOCTEXT:
                    header = group.create_header()
                    printer.header(GROUP_HEADER_SIZE, header)
                    for tag in set(group.tags):
                        printer.label_text(tag)
                
                printer.paragraph_begin()
                for defobj in group.defs:    
                    if defobj.deftype in [DEF.COMMENT, DEF.DOCTEXT]:
                        if in_code_block:
                            printer.code_block(code)
                            in_code_block = False
                            code = ''
                        self.print_comment(printer, defobj.get_text())
                    else:
                        in_code_block = True
                        code += defobj.get_text() + '\n'
                if in_code_block:
                    printer.code_block(code)
                    in_code_block = False
                    code = ''
                printer.paragraph_end()
        
        printer.finalize()
    
    def print_comment(self, printer, comment):
        lines = comment.split('\n')
        line = ''
        code = ''
        
        empty_lines = 0
        in_code_block = False
        
        headers = []
        def _print_header_once(header):
            if header not in headers:
                printer.italic_text(header)
                printer.new_line()
                headers.append(header)
                
        def _print_list_entry(header):
            _print_header_once(PARAM_HEADER)   
            
            name, description = line.split(None, 1)                 
            printer.list_entry()
            printer.bold_text(name)
            
            printer.raw_text(' - ' + description)
        
        for vline in lines:
            # If line ends with backslash, compile them into one
            line += vline
            if vline.endswith('\\'):
                line = line[:-1]
                continue
            
            # Handle code blocks they start with ``` and end with it
            # Directive is taken from MarkDown, but works with other backends
            # i.e. HTML
            if '```' in line:
                if in_code_block:
                    printer.code_block(code)
                    code = ''
                in_code_block = not in_code_block
                line = ''
                continue
                
            if in_code_block:
                code += line + '\n'
                line = ''
                continue
            
            # Do not print too much empty lines
            if not line:
                empty_lines += 1
                if empty_lines > 1:
                    continue
            empty_lines = -1
            
            # Handle directive
            if line.startswith('@'):
                directive, line = line.split(None, 1)
                if directive == '@param':
                    _print_list_entry(PARAM_HEADER)
                elif directive == '@return':
                    _print_header_once(RETVALUE_HEADER)
                    printer.raw_text(line)
                elif directive == '@note':
                    _print_header_once(NOTES_HEADER)
                    printer.raw_text(line)
                elif directive == '@module' or directive == '@name':
                    line = ''
                    continue
                elif directive == '@value':
                    _print_list_entry(VALUE_HEADER)
                elif directive == '@member':
                    _print_list_entry(MEMBER_HEADER)
                else:
                    print line
            else:
                printer.raw_text(line)
            
            printer.new_line()
            line = ''
                
                
            
            
            