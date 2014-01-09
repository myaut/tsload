'''
tsdoc.parser 

implements parsing for C source code into JSON-like representation 
(see tsdoc.Definition and derivatives). It can lately be used to build
documentation and cross-references
'''
import os
import sys
import string
import re

from collections import defaultdict
from tsdoc import *

_DEBUG = False
TRACE_DEFINITIONS = True

__all__ = ['SourceParser', 'TSDocParserError', 'TSDocDefinitionError']

class DEF:
    '''
    Definition hints by SourceParser 
    '''
    NONE    = 0
    COMMENT = 1
    DOCTEXT = 2
    
    PROTO   = 10
    DEFINE  = 11
    VARIABLE = 12
    
    ENUM    = 20
    STRUCT  = 21
    UNION   = 22
    
    TYPEDEF = 30
    
    GLOBALMACRO = 40

class TSDocParserError(Exception):
    pass

class TSDocDefinitionError(Exception):
    def __init__(self, def_parser, msg, *args):
        self.def_parser = def_parser
        
        msg = msg + ' at %s:%d'
        args = args + [defobj.source, defobj.lineno]
        Exception.__init__(msg, *args)

_re_comments = re.compile('\/\*.*\*\/', re.MULTILINE)
def filter_comments(s):
    return _re_comments.sub('', s)

def ptr_to_type(type_name):
    # delete pointer signs
    while '*' in type_name:
        type_name = type_name.strip()
        type_name = type_name[:-1]
        
    return type_name

def is_specifier(token):
    return token in ['const', 'volatile', 'long', 'short', 
                     'signed', 'unsigned', 'extern', 'static',
                     'typedef']

def is_complex_spec(token):
    return token in ['struct', 'union', 'enum']

def is_type_name(type_name):
    type_name = ptr_to_type(type_name)
    return type_name in ['long', 'short', 'signed', 'unsigned',
                         'char', 'int', 'double', 'float', 'wchar_t'] 

_identifier_chars = string.letters + '_'
def is_identifier(token):
    return all(c in _identifier_chars 
               for c in token)

def print_str_markers(s, markers):
    print >> sys.stderr, s.replace('\n', ' ')
    x = [' '] * len(s)
    for (marker, pos) in markers.items():
        if pos != -1: 
            x[pos] = marker[0]
        else:
            x.append(' %s=-1' % marker)
    print >> sys.stderr, ''.join(x)

def strip(line, chars):
    idx  = 0
    ridx = len(line)-1
    
    while idx < len(line) and line[idx] in chars:
        idx += 1
    while ridx > 0 and line[ridx] in chars:
        ridx -= 1
    if idx < len(line) and line[idx] == ' ':
        idx += 1
    
    return line[idx:ridx+1]

class DefinitionParser:
    def __init__(self):
        pass
    
    def parse(self):
        pass
    
    def __repr__(self):        
        text = ''.join(self.lines[:min(len(self.lines), 3)])
        return '<%s DEF.%d @%d "%s">' % (self.__class__.__name__, 
                                         self.deftype, self.lineno, text)

class IdentifierParser:
    def __init__(self, token):
        self.token = token
        
    def parse(self, defobj):
        ''' returns name, type'''
        token = self.token
        rbidx = token.find('[')
        if rbidx > 0:
            defobj.set_name(token[:rbidx])
            defobj.add_type(token[rbidx:])
        elif is_identifier(token):
            defobj.set_name(token)
        else:
            defobj.add_type(token)

class VariableParser(DefinitionParser):
    ''' 
    Variable definition parser. Useful for parsing
    function arguments and variables definitons. 
    
    Tokenizes input, determines type specifiers, types qualifier and 
    variable names. Implementation is pretty heuristical: in some
    contexts 'long' may be specifier and qualifier, in other situations
    'token' may mean structure type name (if prefixed by keyword struct)
    or variable name itself i.e. int token;
    
    parse_variable() handles this situation 
    
    NOTE: doesn't support commas. I.e.:
    int a, b = 10;
    '''
    
    def __init__(self, variable):
        self.variable = filter_comments(variable)
    
    def parse(self):        
        variable = TypeVar()        
        variable.set_value(self._parse_value())
        
        tokens = self.variable.split()
        
        # is_type - next token to be type token
        is_type = True
        # is_complex_type - last token was struct, union or enum, so next token would be
        # last token of type
        is_complex_type = False
        
        # Search for type definition
        for token in tokens:
            if is_specifier(token):
                # Type specifier - add to type tokens
                variable.add_type(token)
                if is_type_name(token):
                    is_type = False
            elif is_complex_type:
                # Previous token was complex specifier - add this token as complex type name
                variable.add_type(token)
                is_complex_type = False
                is_type = False
            elif is_complex_spec(token):
                # Complex type specifier
                variable.add_type(token)
                is_complex_type = True
            elif is_type_name(token):
                # Type qualifier
                variable.add_type(token)
                is_type = False
            elif is_type:
                # Aliased typedef - added as first token (type specifier) 
                variable.add_type(token)
                is_type = False
            else:
                parser = IdentifierParser(token)
                parser.parse(variable)
        
        variable.set_code(self.variable)
        return variable
    
    def _parse_value(self):
        eqidx = self.variable.find('=')
        if eqidx == -1:
            return None
        
        value = self.variable[eqidx+1:]
        self.variable = self.variable[:eqidx]
        
        return value.strip()
        

class FunctionParser(DefinitionParser):
    '''
    Function parser. Works with both function prototypes and pointers to functions
    '''
    def __init__(self, proto):
        self.proto = filter_comments(proto)
    
    def parse(self):
        self.function = Function()
        
        # Function may have __attribute__ at the end of it - cut them out
        for attr in ['CHECKFORMAT', '__attribute__']:
            aidx = self.proto.rfind(attr)  
            if aidx != -1:
                self.proto = self.proto[:aidx]
                
        self._parse_proto(self.proto)
        self.function.set_code(self.proto)
        
        return self.function
    
    def _parse_proto(self, proto):
        lidx = self.proto.index('(')
        ridx = self.proto.rindex(')')
        
        # proto may be pointer to a function and function prototype:
        # int f(void)    - ridx2 == -1 and lidx2 == -1 
        #      l    r   
        # int (*f)  (void)
        #     l  r2 l2   r
        # int f(int (*g)  (void))
        #      l    l2 r2       r
        lidx2 = self.proto.find('(', lidx + 1)
        ridx2 = self.proto.find(')', lidx + 1)
        
        if _DEBUG:
            print_str_markers(proto, {'l': lidx, 'r': ridx, 
                                      'L': lidx2, 'R': ridx2})
        
        if ridx2 < lidx2:
            prefix = proto[:lidx]
            args_str = proto[lidx2+1:ridx]
            
            prefix = prefix.split()
            
            name = proto[lidx+1:ridx2]
        else:        
            prefix = proto[:lidx]
            args_str = proto[lidx+1:ridx]
            
            prefix = prefix.split()
            name = prefix.pop()
            
        name = strip(name, '*')
        self.function.set_name(name)
        
        self._parse_prefix(prefix)
        self._parse_args(args_str)
    
    def _parse_prefix(self, prefix):
        retvalues = []
        is_type = True
        
        while prefix:
            token = prefix[-1]
            
            is_type_token = is_complex_spec(token) or is_type_name(token)
            if not is_type_token and not is_type:
                break
            
            retvalues.append(prefix.pop())
            is_type = False
        
        for retvalue in reversed(retvalues):
            self.function.add_retvalue(retvalue)
        
        self.function.set_specifiers(prefix)
    
    def _parse_args(self, args_str):
        funcptr_arg = False
        funcptr_name = ''
        
        # Parse arguments line. Since it could contain arguments as pointers to
        # a function, which too have commas inside this, simple tokenizer implemented
        # However we do not parse funciton args themselves
        idx = cidx = 0
        have_args = bool(args_str.strip())
        
        is_function_arg = False
        
        while have_args:
            bidx = args_str.find('(', cidx + 1)
            cidx = args_str.find(',', cidx + 1)
            
            is_function_arg = bidx > 0 and bidx < cidx
            
            if is_function_arg:
                # function arguments may contain commas, scan until the end of it
                cidx = self._scan_function_arg(args_str, bidx, cidx)
            
            # Prepend argument
            if cidx == -1:
                # Last argument
                arg_str = args_str[idx:]
                have_args = False
            else:
                # Cut argument and advance position
                arg_str = args_str[idx:cidx]
                idx = cidx + 1
                
            if arg_str == '...':
                continue
            
            if is_function_arg:
                parser = FunctionParser(arg_str)
                function = parser.parse()
                
                arg = TypeVar()
                
                arg.add_type(function)
                arg.set_name(function.name)
            else:
                parser = VariableParser(arg_str)
                arg = parser.parse()
            
            arg.set_class(TypeVar.ARGUMENT)
            self.function.add_arg(arg)
        
    def _scan_function_arg(self, args_str, bidx, cidx):
        rbidx = args_str.find(')', bidx)
        
        braces = 1
        bidx = args_str.find('(', rbidx) + 1
        while braces > 0:
            lbidx = args_str.find('(', bidx)
            rbidx = args_str.find(')', bidx)
            
            if _DEBUG:
                print_str_markers(args_str, {'^': bidx, 'l': lbidx, 'r': rbidx})
                print braces
            
            if rbidx == -1 and lbidx == -1:
                raise TSDocParserError("Invalid argument string '%s'", args_str) 
            
            if lbidx > 0 and (lbidx < rbidx or rbidx == -1):
                braces += 1
                bidx = lbidx + 1
            elif rbidx > 0 and (lbidx > rbidx or lbidx == -1):
                braces -= 1
                bidx = rbidx + 1
        
        return args_str.find(',', bidx)
        
class MultiLineParser(DefinitionParser):
    '''
    Abstract parser that deals with multiple lines. It is fed by SourceParser until 
    it returns True in add_line call which means, that it found end of definition and
    ready to parse it.
    
    To implement end-of-definiton predicate, override is_last_line() 
    To implement parsing function, override multiline_parse() instead of parse() 
    '''
    
    def __init__(self, deftype, source, lineno):
        self.source = source
        self.lineno = lineno
        self.deftype = deftype
        
        self.raw_lines = []
        self.lines = []
    
    def add_line(self, line, raw_line):
        self.raw_lines.append(raw_line)
        self.lines.append(line)
        
        return self.is_last_line(line)
    
    def is_last_line(self, line):
        ''' returns True if definition ends here '''
        return True
    
    def parse(self):
        obj = self.multiline_parse()
        
        if obj is None:
            return None
        
        obj.set_source(self.source, self.lineno)
        return obj
    
    def multiline_parse(self):
        pass

class CommentParser(MultiLineParser):
    '''Parses C comments. 
    
    C comments are starting /* with additional asterisk for documentation
    text. Ignores C comments but parses doctext. Recognizes directive schema
    similiar to doxygen:
    
    param, value or member - for function argument, constant or complex type field
    note, return or see for various types of notes '''
    param_directives = {'@param': DocText.Param.ARGUMENT,
                        '@value': DocText.Param.VALUE,
                        '@member': DocText.Param.MEMBER}
    note_directives = {'@note': DocText.Note.NOTE,
                       '@return': DocText.Note.RETURN,
                       '@see': DocText.Note.REFERENCE}
    
    def is_last_line(self, line):
        return '*/' in line
    
    def multiline_parse(self):
        if self.deftype == DEF.COMMENT:
            return None
        
        param_directives = CommentParser.param_directives
        note_directives = CommentParser.note_directives
        
        doctext = DocText()
        
        line = ''
        text = []
        for vline in self.lines:
            vline = strip(vline, '/*')
            
            # If line ends with backslash, compile them into one
            line += vline
            if vline.endswith('\\'):
                line = line[:-1]
                continue
            
            # Handle directive
            if line.startswith('@'):
                directive, line = line.split(None, 1)
                if directive in param_directives:
                    type = param_directives[directive]
                    name, description = line.split(None, 1)
                    doctext.add_param(type, name, description)
                elif directive in note_directives:
                    type = note_directives[directive]
                    doctext.add_note(type, line)
                elif directive == '@name':
                    doctext.set_name(line)
                elif directive == '@module':
                    doctext.set_module(line)
                else:
                    raise TSDocParserError(self, "Invalid directive '%s'", directive)
            else:
                text.append(line)

            line = ''
        
        text = '\n'.join(text)
        doctext.add_note(DocText.Note.TEXT, text)
        
        return doctext

class TypeDefParser(MultiLineParser):
    '''
    Parses type alias (not complex type, see ComplexTypeParser)
    '''
    def is_last_line(self, line):
        return line.endswith(';')
    
    def multiline_parse(self):
        typedef = ''.join(self.lines)
        typedef = filter_comments(typedef)
        
        if '(' in typedef:
            # Function type definition
            parser = FunctionParser(typedef)
            function = parser.parse()
            
            return function
            
        parser = VariableParser(typedef)
        variable = parser.parse()
        variable.set_class(TypeVar.TYPEDEF)
        
        variable.set_code(self.raw_lines)
        
        return variable
    
class ComplexTypeParser(MultiLineParser):
    '''
    TypeDef aliases and complex type parser (enums, structures and unions)
    
    Parses embedded types too, but still needs better variable/typedef 
    clarification.
    
    FIXME: Fix embedded types and return variables for var declaration
    '''
            
    def __init__(self, deftype, source, lineno):
        MultiLineParser.__init__(self, deftype, source, lineno)
        
        self.braces_count = 0
        self.first_brace = False
        
        self.type_name = ''
    
    def is_last_line(self, line):   
        self.braces_count += line.count('{') - line.count('}')
        if self.braces_count > 0:
            self.first_brace = True
        
        return (self.first_brace and self.braces_count == 0) 

    def multiline_parse(self):
        typedef = ''.join(self.lines)
        typedef = filter_comments(typedef)
        
        return self._parse_complex(typedef)
            
    def _parse_complex(self, typedef):
        lbidx = typedef.index('{')
        rbidx = typedef.rindex('}')
        scidx = typedef.rindex(';')
        
        prefix = typedef[:lbidx]
        body = typedef[lbidx+1:rbidx]
        suffix = typedef[rbidx+1:scidx]
        
        if self.deftype == DEF.ENUM:
            self.type = Enumeration()
            self._parse_values(body)
        else:
            self.type = ComplexType()
            if self.deftype == DEF.STRUCT:
                self.type.set_type(ComplexType.STRUCT)
            elif self.deftype == DEF.UNION:
                self.type.set_type(ComplexType.UNION)
            self._parse_members(body)
        
        parser = VariableParser(prefix)
        typevar = parser.parse()
        
        types = typevar.types
        
        if types[0] == 'typedef':
            aliases = [type_name.strip() 
                       for type_name
                       in suffix.split(',')]
            
            # If it has no name like typedef struct {} name; - ignore it
            # Find typedefed aliases without * or [] and make it primary type name
            if all(is_complex_spec(type_name) or is_specifier(type_name)
                   for type_name in types):
                for alias in aliases[:]:
                    if '[' not in alias and '*' not in alias:
                        aliases.append(types)
                        aliases.remove(alias)
                        types = alias
                        break
        else:
            aliases = [types] 
            # FIXME: need to create variables here
        
        self.type.set_name(types)
        self.type.set_aliases(aliases)
        self.type.set_code(self.raw_lines)
        
        return self.type
    
    def _parse_values(self, values):
        tokens = values.split(',')
        for token in tokens:
            name = token.strip()
            value = None
            
            if '=' in name:
                name, value = name.split('=', 1)
                
                name = name.strip()
                value = value.strip()
            
            valueobj = Value()
            valueobj.set_name(name)
            valueobj.set_value(value)
            
            self.type.add_value(valueobj)
    
    def _parse_members(self, members):
        scidx1 = 0
        scidx2 = 0
        
        while scidx2 != -1:
             scidx2 = members.find(';', scidx1)
             if scidx2 == -1:
                 break
             
             member = members[scidx1:scidx2]
             
             if '{' in member:
                 scidx2 = self._scan_complex_end(members, scidx1)
                 member = members[scidx1:scidx2 + 1]
                 
                 hint = DEF.STRUCT if 'struct' in member else DEF.UNION
                 parser = ComplexTypeParser(hint, '', -1)
                 parser.add_line(member, member)
                 
                 obj = parser.parse()
             else:
                 parser = VariableParser(member)
                 obj = parser.parse()
             
             self.type.add_member(obj)
             scidx1 = scidx2 + 1
    
    def _scan_complex_end(self, members, scidx1):
        scidx2 = members.find('{', scidx1) + 1
        braces = 1
        
        while braces > 0:
            lbidx = members.find('{', scidx2) + 1
            rbidx = members.find('}', scidx2) + 1
            
            if rbidx == -1:
                raise TSDocDefinitionError(self, 'Unfinished curly braces')
            
            if lbidx > 0 and lbidx < rbidx:
                braces += 1
                scidx2 = lbidx + 1
            else:
                braces -= 1
                scidx2 = rbidx + 1
        
        return scidx2
        

class PreprocDefineParser(MultiLineParser):
    def is_last_line(self, line):
        return not line.endswith('\\')
    
    def multiline_parse(self):
        # Merge definition lines and remove backslashes
        line = ''
        
        for vline in self.lines:
            line += vline
            if vline.endswith('\\'):                
                line = line[:-1]
        
        # Cut out #define directive and find proto
        nameidx = line.index('define') + 6
        proto = line[nameidx:]
        
        # Find first name symbol (nameidx)
        nameidx = 0
        while nameidx < len(proto):
            if proto[nameidx] in string.letters or proto[nameidx] == '_':
                break
            nameidx += 1
        else:
            raise TSDocDefinitionError(self, "Invalid preprocessor define '%s'" % proto)
        
        # Find end of name and macro prototype definition
        nameend = None
        end = nameidx
        braces = 0
        while end < len(proto):
            if proto[end] == '(':
                nameend = end
                braces += 1
                self.deftype = DEF.PROTO
            elif proto[end] == ')':
                braces -= 1            
            elif braces == 0 and proto[end] in string.whitespace:
                break
            
            end += 1
        
        if nameend is None:
            # Value-like macro
            name = proto[nameidx:end]            
            value = proto[end:].strip()
            code = '#define ' + proto[nameidx:end] + '\t' + value
            
            obj = Value()
            obj.set_value(value)
        else:
            # Function-like macro
            name = proto[nameidx:nameend]
            code = '#define ' + proto[nameidx:end]
            
            obj = Macro()
            
        obj.set_name(name)
        obj.set_code(code)
        return obj

class BraceCounterMixin:
    def __init__(self, lbrace, rbrace):
        self._lbrace = lbrace
        self._rbrace = rbrace
        
        self.braces_count = 0        
        self.first_brace = False
    
    def is_last_line(self, line):
        if self.braces_count == 0:
            if line.endswith(';'):
                 return True
        
        self.braces_count += line.count(self._lbrace) - line.count(self._rbrace)
        if self.braces_count > 0:
            self.first_brace = True
        
        return (self.first_brace and self.braces_count == 0) 

class PrototypeParser(MultiLineParser, BraceCounterMixin):
    ''' Parses declarations/definitions of functions/variables '''
    def __init__(self, deftype, source, lineno):
        MultiLineParser.__init__(self, deftype, source, lineno)
        BraceCounterMixin.__init__(self, '{', '}')
        
        self.type_name = ''
    
    is_last_line = BraceCounterMixin.is_last_line
    
    def multiline_parse(self):
        proto = ''
        for line in self.lines:
            if line.endswith('{') or line.endswith(';'):
                proto += line[:-1]
                break
            proto += line
        
        if '(' in proto:
            parser = FunctionParser(proto)
            obj = parser.parse()
        else:            
            parser = VariableParser(proto)
            obj = parser.parse()
        
        return obj

class GlobalMacroParser(MultiLineParser, BraceCounterMixin):
    '''
    Some of variables are declared through special macroses that are starting from 
    DECLARE, DEFINE like DECLARE_HASH_MAP. Handle them accurately
    '''
    def __init__(self, deftype, source, lineno):
        MultiLineParser.__init__(self, deftype, source, lineno)
        BraceCounterMixin.__init__(self, '(', ')')
    
    is_last_line = BraceCounterMixin.is_last_line
     
    def multiline_parse(self):
        return None

class DefinitionGroupParser:
    def __init__(self):
        self.defs = []
        
        self.def_parser = None
        self.deftype = DEF.NONE
    
    def parse_line(self, source, lineno, raw_line):
        '''returns True if definition finished it's parsing'''
        line = raw_line.strip()
        
        def_parser = self.def_parser
             
        if def_parser is None:
            if line.startswith('/**'):
                def_parser = CommentParser(DEF.DOCTEXT, source, lineno)
            elif line.startswith('/*'):
                def_parser = CommentParser(DEF.COMMENT, source, lineno)
            elif line.startswith('#'):
                line1 = line[1:].strip()
                if line1.startswith('define'):
                    def_parser = PreprocDefineParser(DEF.DEFINE, source, lineno)
            elif 'struct' in line:
                def_parser = ComplexTypeParser(DEF.STRUCT, source, lineno)
            elif 'union' in line:
                def_parser = ComplexTypeParser(DEF.UNION, source, lineno)
            elif 'enum' in line:
                def_parser = ComplexTypeParser(DEF.ENUM, source, lineno)
            elif 'typedef' in line:
                def_parser = TypeDefParser(DEF.TYPEDEF, source, lineno)
            elif line.startswith('DECLARE') or line.startswith('DEFINE'):
                def_parser = GlobalMacroParser(DEF.GLOBALMACRO, source, lineno)
            else:
                def_parser = PrototypeParser(DEF.PROTO, source, lineno)
        
        if def_parser is not None:
            if def_parser.add_line(line, raw_line):
                self._add_defobj(def_parser)
                def_parser = None
        
        self.def_parser = def_parser
        return def_parser is None
    
    def _add_defobj(self, def_parser):
        defobj = def_parser.parse()
        
        if defobj is None:
            return
        
        if TRACE_DEFINITIONS:
            source = os.path.basename(defobj.source)
            line = '\t%s\t%s\t@%s:%d' % (defobj.__class__.__name__.upper(),
                                        defobj.name, source, defobj.lineno)
            print >> sys.stderr, line
        
        if isinstance(defobj, list):
            self.defs.extend(defobj)
        else:
            self.defs.append(defobj)
            
    def get_group(self):
        if not self.defs:
            return None
        
        return DefinitionGroup(self.defs)
    
    def __repr__(self):
        names = self.get_names(False) or hex(id(self))
        return '<DefinitionGroup %s>' % names

class SourceParser:
    def __init__(self, path):
        self.is_header = path.endswith('.h')        
        self.path = path
        
        # Transform 'include/threads.h' into 'threads' 
        filename = os.path.basename(path)
        self.module = filename[:filename.rindex('.')] 
        
        self.title = self.module
        self.default_title = True
        
        self.source = file(path, 'r')
        
        self.groups = []
        self.defs = []
        
    def parse(self):
        lineno = 1
        group_parser = DefinitionGroupParser()
        lastline = True
        
        for raw_line in self.source:
            empty = all(c in string.whitespace for c in raw_line)
            
            if lastline and empty:
                self._add_group(group_parser)  
                group_parser = DefinitionGroupParser()
            else:
                lastline = group_parser.parse_line(self.path, lineno, raw_line)
                
            lineno += 1
        
    def _add_group(self, group_parser):
        group = group_parser.get_group()
        
        if group is not None:
            self.groups.append(group)
    
    def serialize(self):
        group_list = []
        for group in self.groups:
            group_list.append(group.serialize())
        
        return group_list 
    
    def merge(self, other):
        for group in other.groups:
            names = group.get_names()
            
            for my_group in self.groups:
                my_names = group.get_names()
                if my_names & names:
                    my_group.merge(group)
                    break
            else:
                if any(isinstance(defobj, DocText) 
                       for defobj in group.defs):
                    self.groups.append(group)
        
        

if __name__ == '__main__':
    import pprint
    
    def header(test, text):
        print '\n= %s =' % test
        print text
        print '-------------------'
    
    def assert_typevar(variable, types, name, value = None):
        header('assert_typevar', variable)
        
        # parse
        parser = VariableParser(variable)
        varobj = parser.parse()
        
        # dump
        pprint.pprint(varobj.serialize())
        
        # assert
        assert varobj.name == name
        assert varobj.types == types
        assert varobj.value == value
    
    def assert_function(function, name, retvalue, specifiers, args):
        header('assert_function', function)
        
        # parse
        parser = FunctionParser(function)
        funcobj = parser.parse()
        
        # dump
        pprint.pprint(funcobj.serialize())
        
        assert funcobj.name == name
        assert funcobj.retvalue == retvalue
        assert funcobj.specifiers == specifiers
        
        for funcarg, arg in zip(funcobj.args, args):
            types, name = arg 
            
            if types:
                assert types == types
            assert funcarg.name == name
    
    def assert_complex(complex, hint):
        header('assert_complex', complex)
        
        # parse
        parser = ComplexTypeParser(hint, 'test.c', 0)
        parser.add_line(complex, complex)
        
        typeobj = parser.parse()
        
        # dump
        pprint.pprint(typeobj.serialize())
        
        # assert
    
    
    def assert_enum(enum, name, values):
        header('assert_enum', enum)
        
        # parse
        parser = ComplexTypeParser(DEF.ENUM, 'test.c', 0)
        parser.add_line(enum, enum)
        
        typeobj = parser.parse()
        
        # dump
        pprint.pprint(typeobj.serialize())
        
        # assert
        assert typeobj.name == name
        for (name, value) in values:
            for enumvalue in typeobj.values:
                if enumvalue.name == name:
                    assert enumvalue.value == value
                    break
            else:
                raise KeyError(name)
    
    assert_typevar('const long int i', ['const', 'long', 'int'], 'i')
    assert_typevar('long i', ['long'], 'i')
    assert_typevar('long int', ['long', 'int'], '')
    assert_typevar('struct S', ['struct', 'S'], '')
    assert_typevar('volatile struct S* s', ['volatile', 'struct', 'S*'], 's')
    assert_typevar('type_t t',  ['type_t'], 't')
    assert_typevar('type_t t /* t */',  ['type_t'], 't')
    assert_typevar('typedef struct S s_t',  ['typedef', 'struct', 'S'], 's_t')
    assert_typevar('typedef int array[10]',  ['typedef', 'int', '[10]'], 'array')
    
    assert_typevar('long int array[3] = {0, 0, 1}', ['long', 'int', '[3]'], 'array',
                    '{0, 0, 1}')
        
    assert_function('''struct S* do_things(void)''', 
                'do_things', ['struct', 'S*'], [], [('void', '')])
    assert_function('''short int ooooooooo(/* long z */ int i)''', 
                'ooooooooo', ['short', 'int'], [], [('int', 'i')])
    assert_function('''LIBEXPORT int f(void (*g)(void*), 
        int a, int (* mod_config_func)(struct module* mod, void (*h)(void*)))''', 
            'f', ['int'], ['LIBEXPORT'], [([], 'g'), (['int'], 'a'), ([], 'mod_config_func')])
    
    assert_complex('''struct S { int a; };''', DEF.STRUCT)
    assert_complex('''typedef struct S { int a; } tS;''', DEF.STRUCT)
    assert_complex('''typedef union { int a; } tS;''', DEF.UNION)
    assert_complex('''typedef struct { union { int a; char c; } hdr; char str[128]; } tS;''', DEF.STRUCT)
    
    assert_enum('''enum error { E_OK = -1, E_NOT_OK } variable;''', ['enum', 'error'],
                    [('E_OK', '-1'), ('E_NOT_OK', None)])
    assert_enum('''typedef enum { E_OK = -1, E_NOT_OK } error_t;''', 'error_t',
                    [('E_OK', '-1'), ('E_NOT_OK', None)])
    