'''
Created on Jul 9, 2013

@author: myaut
'''

from tsload.jsonts.api.load import *

class WLParamHelper:
    typeMap = {TSWLParamBoolean: 'bool',
               TSWLParamInteger: 'int',
               TSWLParamSize: 'size',
               TSWLParamTime: 'time',
               TSWLParamFloat: 'float',
               TSWLParamString: 'str',
               TSWLParamFilePath: 'path',
               TSWLParamDisk: 'disk',
               TSWLParamCPUObject: 'cpuobject',
               TSWLParamStringSet: 'strset'
               }
    
    @staticmethod
    def getTypeName(wlp):
        '''Returns string short name'''
        return WLParamHelper.typeMap[wlp.__class__]
    
    @staticmethod
    def getIntegerRange(wlp):
        '''Returns pair of minimum or maximum values of parameter 
        descriptor or pair of None if not defined'''
        if isinstance(wlp, TSWLParamInteger):
            minVal = wlp.getopt('min', None)
            maxVal = wlp.getopt('max', None)
            
            return minVal, maxVal
        return None, None
    
    @staticmethod
    def getStringLength(wlp):
        '''Returns maximum length of string or None if not defined'''
        if isinstance(wlp, TSWLParamString):
            return  wlp.len
        return None
    
    @staticmethod
    def getDefaultValue(wlp):
        '''Returns default value of parameter or None if not defined'''
        if isinstance(wlp, TSWLParamInteger) or \
            isinstance(wlp, TSWLParamBoolean) or \
            isinstance(wlp, TSWLParamFloat) or \
            isinstance(wlp, TSWLParamString) or \
            isinstance(wlp, TSWLParamStringSet):
                # all of WL parameter types support default, but couldn't be sure
                return wlp.getopt('default', None)
        return None