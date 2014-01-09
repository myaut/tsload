import json
import string

class Definition:
    def __init__(self):
        self.code = ''
        self.name = ''
        self.tags = []
        
        self.source = ''
        self.lineno = -1
    
    def set_source(self, source, lineno):
        self.source = source
        self.lineno = lineno
        
    def set_name(self, name):
        if isinstance(name, str):
            name = name.strip()
        self.name = name
        
    def set_code(self, code):
        self.code = code
    
    @staticmethod
    def _serialize_object(value):
        if isinstance(value, list):
            return map(Definition._serialize_object, value)
        elif isinstance(value, Definition):
            return value.serialize(is_root = False)
        
        return value
    
    def serialize(self, is_root = True):
        klass = self.__class__
        object = {'class': klass.__name__,
                  'name': self.name}
        
        if is_root:
            object.update({'code': self.code,
                           'source': self.source,
                           'lineno': self.lineno})
        
        for field in klass.__tsdoc_fields__:
            value = getattr(self, field)
            object[field] = Definition._serialize_object(value)
        
        return object
        
class DocText(Definition):
    class Param(Definition):
        __tsdoc_fields__ = ['type', 'name', 'description']
        
        ARGUMENT = 0
        MEMBER   = 1
        VALUE    = 2
        
        def __init__(self, type, name, description):
            Definition.__init__(self)
            
            self.type = type
            self.name = name
            self.description = description
    
    class Note(Definition):
        __tsdoc_fields__ = ['type', 'note']
        
        TEXT      = 0
        NOTE      = 1
        RETURN    = 2
        REFERENCE = 3
        
        def __init__(self, type, note):
            Definition.__init__(self)
            
            self.type = type
            self.note = note
    
    __tsdoc_fields__ = ['module', 'params', 'notes']
    
    def __init__(self):
        Definition.__init__(self)
        
        self.module = None
        self.params = [] 
        self.notes = []
    
    def add_param(self, type, name, description):
        self.params.append(DocText.Param(type, name, description))
    
    def add_note(self, type, note):
        self.notes.append(DocText.Note(type, note))
    
    def set_module(self, module):
        self.module = module
        
class TypeVar(Definition):
    __tsdoc_fields__ = ['types', 'tv_class', 'value']
    
    VARIABLE = 0
    ARGUMENT = 1
    TYPEDEF = 2
    
    def __init__(self):
        Definition.__init__(self)
        
        self.types = []
        self.value = None
        self.tv_class = TypeVar.VARIABLE
    
    def add_type(self, name):
        self.types.append(name)
        
    def set_value(self, value):
        self.value = value
        
    def set_class(self, tv_class):
        self.var_class = tv_class
        
class Function(Definition):
    __tsdoc_fields__ = ['retvalue', 'args', 'specifiers']
    
    def __init__(self):
        Definition.__init__(self)
        
        self.retvalue = []
        self.args = []
        self.specifiers = []
    
    def add_arg(self, arg):
        self.args.append(arg)
    
    def add_retvalue(self, retvalue):
        self.retvalue.append(retvalue)
        
    def set_specifiers(self, specifiers):
        self.specifiers = specifiers

    def add_type(self, type):
        self.types.append(type)

class Macro(Definition):
    __tsdoc_fields__ = []

class Enumeration(Definition):    
    __tsdoc_fields__ = ['values', 'aliases']
    
    def __init__(self):
        Definition.__init__(self)
        
        self.values = []
        self.aliases = []
    
    def add_value(self, value):
        self.values.append(value)
    
    def set_aliases(self, aliases):
        self.aliases = aliases

class ComplexType(Definition):
    __tsdoc_fields__ = ['members', 'aliases', 'type']
    
    STRUCT = 1
    UNION = 2
    
    def __init__(self):
        Definition.__init__(self)
        
        self.type = 0
        self.members = []
        self.aliases = []
    
    def set_type(self, type):
        self.type = type
    
    def add_member(self, member):
        self.members.append(member)
        
    def set_aliases(self, aliases):
        self.aliases = aliases
        
class Value(Definition):
    __tsdoc_fields__ = ['value']
    
    def __init__(self):
        Definition.__init__(self)
        
        self.value = None
    
    def set_value(self, value):
        self.value = value

class DefinitionGroup:
    def __init__(self, defs):
        self.defs = defs
    
    def get_names(self):
        names = []
        
        for defobj in self.defs:
            name = defobj.name
            if isinstance(name, list):
                name = ' '.join(name)
            if name:
                names.append(name)
        
        return set(names)
    
    def merge(self, other):
        for defobj in other.defs:
            if isinstance(defobj, DocText):
                self.defs.insert(0, defobj)
    
    def serialize(self):
        def_list = []
        for defobj in self.defs:
            def_list.append(defobj.serialize())
        
        return def_list 