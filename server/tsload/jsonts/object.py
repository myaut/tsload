'''
TSObject serializer/deserializer

Example:

class TSIntObject(TSObject):
    id = TSObject.Int()

class TSMyObject(TSObject):
    id = TSObject.Int()
    
    names = TSObject.Array(TSObject.String())      
    types = TSObject.Map(TSObject.Object(TSIntObject))
'''

import functools
from tsload.jsonts import JSONTS

class TSObject:
    class Type:
        pass
    
    class Any:
        def deserialize(self, val):
            return val
        
        serialize = deserialize
    
    class Null:
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
    
    @classmethod
    def deserialize(cls, val):
        obj = cls()
        
        for fieldName in dir(cls):
            field = getattr(cls, fieldName)
            
            if isinstance(field, TSObject.Type):
                setattr(obj, fieldName, field.deserialize(val[fieldName]))
                
        return obj
                
    def serialize(self):
        val = {}
        
        for fieldName in dir(self.__class__):
            field = getattr(self, fieldName)
            fieldClass = getattr(self.__class__, fieldName)
            
            if isinstance(fieldClass, TSObject.Type):
                val[fieldName] = fieldClass.serialize(field)
                
        return val
    
if __name__ == '__main__':
    import pprint
    
    class TSIntObject(TSObject):
        id = TSObject.Int()
    
    class TSMyObject(TSObject):
        id = TSObject.Int()
        
        names = TSObject.Array(TSObject.String())      
        types = TSObject.Map(TSObject.Object(TSIntObject))
        
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
        