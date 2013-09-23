'''
TSObject serializer/deserializer

Example:

class TSIntObject(TSObject):
    id = TSObject.Int()

class TSMyObject(TSObject):
    id = TSObject.Int()
    
    names = TSObject.Array(TSObject.String())      
    types = TSObject.Map(TSObject.Object(TSIntObject))
    
    opt = TSObject.Optional(TSObject.Int())
'''

import functools
from tsload.jsonts import JSONTS

class TSObject:
    @staticmethod
    def Optional(tsoType):
        tsoType.optional = True
        return tsoType
    
    class Undef:
        pass
    
    class Type:
        optional = False
    
    class Any(Type):
        def deserialize(self, val):
            return val
        
        serialize = deserialize
    
    class Null(Type):
        def deserialize(self, val):
            if val is not None:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT, 
                                   "Expected Null, got %s" % type(val))
        
        serialize = deserialize 
    
    # For simple types all (de-)serialization is made by JSON module, we only check types
    
    class SimpleType(Type):
        def deserialize(self, val):
            if type(val) != self.objType:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT, 
                                   "Expected %s, got %s" % (self.objType, type(val)))
            
            return val
        
        def serialize(self, val):
            return val
    
    class Int(SimpleType):
        objType = int
    
    class Float(SimpleType):
        objType = float
        
    class Bool(SimpleType):
        objType = bool
    
    class String(SimpleType):
        def serialize(self, val):
            return str(val)
        
        deserialize = serialize
    
    # Containers
    
    # Containers are deserialized by JSON:
    #    ARRAY => list(value)
    #    NODE => dict(str: value)
    # We only need to serialize/deserialize values, but list and dict
    # have different comprehensions, so implement comprehension in serdes func
    # and make it partially applied
    
    class ContainerType(Type):
        def __init__(self, elType):
            self.serialize = functools.partial(self.serdes, elType.serialize)
            self.deserialize = functools.partial(self.serdes, elType.deserialize)
    
    class Array(ContainerType):    
        def serdes(self, serdesFunc, val):
            return [serdesFunc(v)
                    for v in val]
        
    
    class Map(ContainerType):            
        def serdes(self, serdesFunc, val):
            return dict((k, serdesFunc(v))
                        for k, v in val.iteritems())
    
    # Objects
    
    class Object:
        'Adapter for TSObject instances'
        def __init__(self, objClass):
            self.objClass = objClass
        
        def deserialize(self, val):
            return self.objClass.deserialize(val)
        
        def serialize(self, val):
            return val.serialize()
    
    class MultiObject:
        '''Depending on field value named field, it constructs one of the classes
        listed in objClassMap. '''
        def __init__(self, field, objClassMap):
            self.field = field
            self.objClassMap = objClassMap
            self.revClassMap = dict((v, k) for k, v in objClassMap.items())
        
        def deserialize(self, val):
            fieldValue = val.get(self.field, TSObject.Undef)
            
            if fieldValue is TSObject.Undef:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                   'Missing field "%s" for multiobject' % (self.field))
            
            try:
                objClass = self.objClassMap[fieldValue]
            except KeyError:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                   'Invalid field "%s" value "%s" for multiobject: Unknown class' % (self.field,
                                                                                                     fieldValue))
            
            return objClass.deserialize(val)
        
        def serialize(self, val):
            try:
                objType = self.revClassMap[val.__class__]
            except KeyError:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                   'Invalid class "%s" for multiobject: Unknown type' % (val.__class__.__name__))
            
            obj = val.serialize()
            obj[self.field] = objType
            
            return obj
    
    def getopt(self, optName, default = Undef):
        '''Return optional value or default.
        Works like getattr, but do not search class (and then return tso.Type)
        
        raises AttributeError of invalid optName provided or it is not optional field
        raises ValueError if default argument and value are not set'''
        field = getattr(self.__class__, optName)
        
        if not field.optional:
            # Internal error
            raise AttributeError('Attribute "%s" is not optional' % optName)
        
        ret = self.__dict__.get(optName, default)
        
        if ret is TSObject.Undef:
            raise ValueError('Default argument and value are not set')
        
        return ret
    
    class Nullable(Type):
        def __init__(self, tso):
            self.tso = tso
        
        def serialize(self, val):
            if val is None:
                return None
            
            return self.tso.serialize(val)
        
        def deserialize(self, val):
            if val is None:
                return None
            
            return self.tso.deserialize(val)
    
    @classmethod
    def deserialize(cls, val):
        obj = cls()
        
        for fieldName in dir(cls):
            field = getattr(cls, fieldName)
            
            if isinstance(field, TSObject.Type):
                fieldValue = val.get(fieldName, TSObject.Undef)
                
                if fieldValue is not TSObject.Undef:
                    fieldDeserVal = field.deserialize(fieldValue)
                    setattr(obj, fieldName, fieldDeserVal)
                elif not field.optional:
                    raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                       'Missing field "%s" for class "%s"' % (fieldName, cls.__name__))
                         
        return obj
                
    def serialize(self):
        val = {}
        
        for fieldName in dir(self.__class__):
            field = getattr(self, fieldName)
            fieldClass = getattr(self.__class__, fieldName)
            
            if isinstance(fieldClass, TSObject.Type):
                if isinstance(field, TSObject.Type):
                    if field.optional:
                        # Do not serialize optional values
                        continue
                    else:
                        raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                           'Field "%s" for class "%s" is not set' % (fieldName, 
                                                                                     self.__class__.__name__))
                try:
                    val[fieldName] = fieldClass.serialize(field)
                except Exception, e:
                    raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT,
                                       'Field "%s" for class "%s" serialization failed: "%s"' % (fieldName, 
                                                                                                 self.__class__.__name__,
                                                                                                 str(e)))
                
        return val
    
if __name__ == '__main__':
    import pprint
    
    class TSIntObject(TSObject):
        id = TSObject.Int()
    
    class TSMyObject(TSObject):
        id = TSObject.Int()
        
        names = TSObject.Array(TSObject.String())      
        types = TSObject.Map(TSObject.Object(TSIntObject))
        
        opt = TSObject.Optional(TSObject.Int())
        
    i1 = TSIntObject()
    i1.id = 10
    
    i2 = TSIntObject()
    i2.id = 20
    
    my = TSMyObject()
    
    my.id = 100
    my.names = ['Petr', 'Anna']
    my.types = {'Petr': i1, 'Anna': i2}
    
    ser = my.serialize()
    
    pprint.pprint(ser)
    
    my2 = TSMyObject.deserialize(ser)
        