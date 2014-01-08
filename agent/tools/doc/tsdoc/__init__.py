import string

class DEF:
    NONE    = 0
    COMMENT = 1
    DOCTEXT = 2
    
    PROTO   = 10
    DEFINE  = 11
    
    ENUM    = 20
    STRUCT  = 21
    TYPEDEF = 22

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

def print_str_markers(s, markers):
    print s.replace('\n', ' ')
    x = [' '] * len(s)
    for (marker, pos) in markers.items():
        if pos != -1: 
            x[pos] = marker[0]
        else:
            x.append(' %s=-1' % marker)
    print ''.join(x)

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

def parse_variable(variable):
    tokens = variable.split()
    type_tokens = []
    name_tokens = []
    is_type = True
    is_complex_type = False
    
    # Search for type definition
    for token in tokens:
        if is_specifier(token):
            type_tokens.append(token)
            if is_type_name(token):
                is_type = False
        elif is_complex_type:
            type_tokens.append(token)
            is_complex_type = False
            is_type = False
        elif is_complex_spec(token):
            type_tokens.append(token)
            is_complex_type = True
        elif is_type_name(token):
            type_tokens.append(token)
            is_type = False
        elif is_type:
            type_tokens.append(token)
            is_type = False
        else:
            name_tokens.append(token)
    
    if not name_tokens:
        var_name = ''
    else:
        var_name = name_tokens[0]
    
    return (type_tokens, var_name)

def parse_function(proto):
    lidx = proto.index('(')
    ridx = proto.rindex(')')
    
    # proto may be pointer to a function and function prototype:
    # int f(void)    - ridx2 == -1 and lidx2 == -1 
    #      l    r   
    # int (*f)  (void)
    #     l  r2 l2   r
    # int f(int (*g)  (void))
    #      l    l2 r2       r
    lidx2 = proto.find('(', lidx + 1)
    ridx2 = proto.find(')', lidx + 1)
    
    # print_str_markers(proto, {'l': lidx, 'r': ridx, 
    #                          'L': lidx2, 'R': ridx2})
    
    if ridx2 < lidx2:
        prefix = proto[:lidx]
        argsstr = proto[lidx2+1:ridx]
        
        prefix = prefix.split()
        name = proto[lidx+1:ridx2]
    else:        
        prefix = proto[:lidx]
        argsstr = proto[lidx+1:ridx]
        
        prefix = prefix.split()
        name = prefix.pop()
    
    retvalue = prefix.pop()
    specifiers = prefix
    
    args = []
    
    funcptr_arg = False
    funcptr_name = ''
    
    # Parse arguments line. Since it could contain arguments as pointers to
    # a function, which too have commas inside this, simple tokenizer implemented
    # However we do not parse funciton args themselves
    idx = cidx = 0
    have_args = bool(argsstr.strip())
    while have_args:
        bidx = argsstr.find('(', cidx + 1)
        cidx = argsstr.find(',', cidx + 1)
        
        if bidx > 0 and bidx < cidx:
            rbidx = argsstr.find(')', bidx)
            funcptr_name = argsstr[bidx:rbidx]
            funcptr_name = strip(funcptr_name, '(*')
            
            braces = 1
            bidx = argsstr.find('(', rbidx) + 1
            while braces > 0:
                lbidx = argsstr.find('(', bidx)
                rbidx = argsstr.find(')', bidx)
                
                if False:
                    print_str_markers(argsstr, {'^': bidx, 'l': lbidx, 'r': rbidx})
                    print braces
                
                if rbidx == -1 and lbidx == -1:
                    raise ValueError("Invalid argument string '%s'" % argsstr) 
                
                if lbidx > 0 and (lbidx < rbidx or rbidx == -1):
                    braces += 1
                    bidx = lbidx + 1
                elif rbidx > 0 and (lbidx > rbidx or lbidx == -1):
                    braces -= 1
                    bidx = rbidx + 1
        
            cidx = argsstr.find(',', bidx)
            funcptr_arg = True
            
        if cidx == -1:
            arg = argsstr
            have_args = False
        else:
            arg = argsstr[idx:cidx]
            idx = cidx + 1
            
        if arg == '...':
            continue
        
        if funcptr_arg:
            funcptr_arg = False
            args.append(([], funcptr_name))
        else:    
            args.append(parse_variable(arg))
    
    return name, retvalue, args, specifiers 

class Definition:
    def __init__(self, deftype, lineno):
        self.lineno = lineno
        self.deftype = deftype
        
        self.raw_lines = []
        self.lines = []
        
        self.name = ''
        self.text = ''
        
        self.type_refs = []
        
        self.public = False
    
    def add_line(self, line, raw_line):
        self.raw_lines.append(raw_line)
        self.lines.append(line)
        
        return self.is_last_line(line)
    
    def _add_type_ref(self, type_name):
        type_name = ptr_to_type(type_name)
        self.type_refs.append(type_name)
    
    def is_last_line(self, line):
        ''' returns True if definition ends here '''
        return True
    
    def parse(self):
        pass
    
    def __repr__(self):        
        text = ''.join(self.lines[:min(len(self.lines), 3)])
        return '<%s @%d "%s">' % (self.__class__.__name__, self.lineno, text)
    
    def get_text(self):
        return self.text

class Comment(Definition):
    def __init__(self, deftype, lineno):
        Definition.__init__(self, deftype, lineno)
        
        if deftype == DEF.DOCTEXT:
            self.public = True
    
    def is_last_line(self, line):
        return line.endswith('*/')
    
    def parse(self):
        lines = []
        empty_line = False
        empty_prev = False
        
        for line in self.lines:
            line = strip(line, '/*')
            
            empty_line = len(line) < 0            
            if empty_line and empty_prev:
                continue
            empty_prev = empty_line
            
            lines.append(line)
        
        self.text = '\n'.join(lines) 
            
class TypeDef(Definition):        
    def __init__(self, deftype, lineno):
        Definition.__init__(self, deftype, lineno)
        
        self.braces_count = 0
        self.first_brace = False
        
        self.type_name = ''
    
    def is_last_line(self, line):
        if self.deftype == DEF.TYPEDEF:
            return True
                
        self.braces_count += line.count('{') - line.count('}')
        if self.braces_count > 0:
            self.first_brace = True
        
        return (self.first_brace and self.braces_count == 0) 
    
    def parse(self):
        typedef = ''.join(self.lines)
        
        if self.deftype == DEF.TYPEDEF:
            if '(' in typedef:
                # Function type definition
                name, retvalue, args, specifiers = parse_function(typedef)
                self.type_name = strip(name, '(*')
            else:
                lst = typedef.split()
                self.type_name = strip(lst[-1], ';')
            
            self.aliases = [self.type_name]
            self.name = self.type_name
            self.text = typedef
        else:
            lbidx = typedef.index('{')
            rbidx = typedef.rindex('}')
            scidx = typedef.rindex(';')
            
            prefix = typedef[:lbidx]
            body = typedef[lbidx+1:rbidx]
            suffix = typedef[rbidx+1:scidx]
            
            types, _ = parse_variable(prefix)
            
            if types[0] == 'typedef':
                types.pop(0)
                aliases = [type_name.strip() 
                           for type_name
                           in suffix.split(',')]
            else:
                aliases = [] 
            
            # If it has no name like typedef struct {} name; - ignore it
            if all(is_complex_spec(type_name) or is_specifier(type_name)
                   for type_name in types):
                name = ''
            else:
                name = ' '.join(types)     
            
            for alias in aliases[:]:
                # Find typedefed aliases without * or []
                # and make it primary type name
                if '[' not in alias and '*' not in alias:
                    aliases.append(name)
                    aliases.remove(alias)
                    name = alias
                    break
            
            self.aliases = aliases
            self.name = name
            self.text = ''.join(self.raw_lines)

class PreprocDefine(Definition):
    def is_last_line(self, line):
        return not line.endswith('\\')
    
    def parse(self):
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
            raise ValueError("Invalid preprocessor define '%s'" % proto)
        
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
            # Value-like macros
            self.name = proto[nameidx:end]
            
            value = proto[end:].strip()
            self.text = '#define ' + proto[nameidx:end] + ' ' + value
        else:
            # Function-like macros
            self.name = proto[nameidx:nameend]
            self.text = '#define ' + proto[nameidx:end]
            

class Prototype(Definition):
    def __init__(self, deftype, lineno):
        Definition.__init__(self, deftype, lineno)
        
        self.braces_count = 0        
        self.first_brace = False
        self.type_name = ''
    
    def is_last_line(self, line):
        if self.braces_count == 0:
            if line.endswith(';'):
                 return True
        
        self.braces_count += line.count('{') - line.count('}')
        if self.braces_count > 0:
            self.first_brace = True
        
        return (self.first_brace and self.braces_count == 0) 
            
    def parse(self):
        proto = ''
        for line in self.lines:
            if line.endswith('{') or line.endswith(';'):
                proto += line[:-1]
                break
            proto += line
        
        try:
            name, retvalue, args, specifiers = parse_function(proto)
            
            self.public =  'LIBEXPORT' in specifiers
            self.platfunc = 'PLATAPI' in specifiers
            
            self._add_type_ref(retvalue)
            for (types, arg_name) in args:
                if types:
                    self._add_type_ref(types[-1])
            
            self.is_function = True
            
            idx = proto.index(retvalue)
            self.name = name
            self.text = proto[idx:]
        except ValueError:
            self.is_function = False
            
            types, var_name = parse_variable(proto)
            
            if types:
                self._add_type_ref(types[-1])
            
            self.name = var_name
            self.text = proto 
        
class DefinitionGroup:
    def __init__(self):
        self.lines = []
        self.defs = []
        
        self.leader = None
        self.defobj = None
        self.deftype = DEF.NONE
        
        self.header = None
    
    def parse_line(self, lineno, raw_line):
        '''returns True if definition finished it's parsing'''
        line = raw_line.strip()
        self.lines.append(raw_line)
        
        defobj = self.defobj
             
        if defobj is None:
            if line.startswith('/**'):
                defobj = Comment(DEF.DOCTEXT, lineno)
            elif line.startswith('/*'):
                defobj = Comment(DEF.COMMENT, lineno)
            elif line.startswith('#'):
                line1 = line[1:]
                if line1.startswith('define'):
                    defobj = PreprocDefine(DEF.DEFINE, lineno)
            elif 'struct' in line or 'union' in line:
                defobj = TypeDef(DEF.STRUCT, lineno)
            elif 'enum' in line:
                defobj = TypeDef(DEF.ENUM, lineno)
            elif 'typedef' in line:
                defobj = TypeDef(DEF.TYPEDEF, lineno)
            else:
                # May be both function or extvar
                defobj = Prototype(DEF.PROTO, lineno)
        
        if defobj is not None:
            if defobj.add_line(line, raw_line):
                defobj.parse()
                self.defs.append(defobj)
                defobj = None
        
        self.defobj = defobj
        return defobj is None
    
    def is_public(self):
        return any(defobj.public or defobj.deftype == DEF.DOCTEXT
                   for defobj in self.defs)
    
    def get_defclass(self):
        deftypes = [defobj.deftype for defobj in self.defs]
        
        if not deftypes:
            return DEF.NONE
        
        max_deftype = max(deftypes)
        
        if max_deftype <= DEF.DOCTEXT:
            return max_deftype
        elif max_deftype == DEF.PROTO:
            return DEF.PROTO
        elif max_deftype == DEF.DEFINE:
            return DEF.DEFINE
        
        return DEF.TYPEDEF
    
    def get_names(self, public = True):
        names = []
        for defobj in self.defs:
            if (defobj.public or not public) and defobj.name:
                names.append(defobj.name)
                
        return names
    
    def create_header(self):
        if self.header is not None:
            return self.header
        
        names = self.get_names()
        if not names:
            # Try not public names
            names = self.get_names(False)
                
        return  ', '.join(names)
    
    def merge(self, group, names):
        merged = 0
        new_group = None
        
        for defobj in self.defs[:]:
            if defobj.name in names:
                new_group = DefinitionGroup()
                mark = False 
                added = 0
                
                for def2 in reversed(group.defs):
                    if mark and def2.deftype == DEF.DOCTEXT:
                        new_group.defs.append(def2)
                        added += 1
                    
                    if def2.name == defobj.name:
                        mark = True
                        continue
                    elif mark:
                        break
                
                if added == 0:
                    return None
                
                self.defs.remove(defobj)
                new_group.defs.append(defobj)
        
        return new_group

if __name__ == '__main__':
    assert parse_variable('const long int i') == (['const', 'long', 'int'], 'i')
    assert parse_variable('long i') == (['long'], 'i')
    assert parse_variable('long int') == (['long', 'int'], '')
    assert parse_variable('struct S') == (['struct', 'S'], '')
    assert parse_variable('volatile struct S* s') == (['volatile', 'struct', 'S*'], 's')
    assert parse_variable('type_t t') == (['type_t'], 't')
    assert parse_variable('type_t t /* t */') == (['type_t'], 't')
    
    name, retvalue, args, specifiers = parse_function('''LIBEXPORT int f(void (*g)(void*), 
        int a, int (* mod_config_func)(struct module* mod, void (*h)(void*)))''')
    
    assert name == 'f'
    assert retvalue == 'int'
    assert specifiers == ['LIBEXPORT']
    assert args == [([], 'g'), (['int'], 'a'), ([], 'mod_config_func')]
            