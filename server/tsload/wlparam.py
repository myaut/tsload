'''
Created on Jul 9, 2013

@author: myaut
'''

from tsload.jsonts.api.load import *

from tsload.util.format import parseSizeParam, parseTimeParam

class WLParamRangeError(Exception):
    pass

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
    
    @staticmethod
    def parseParamValue(wlp, value):
        if isinstance(wlp, TSWLParamBoolean):
            return bool(value)
        elif isinstance(wlp, TSWLParamInteger):
            if isinstance(wlp, TSWLParamSize):
                val = parseSizeParam(value)
            elif isinstance(wlp, TSWLParamTime):
                val = parseTimeParam(value)
            else:
                val = int(value)
            
            minVal, maxVal = WLParamHelper.getIntegerRange(wlp)
            
            if (maxVal is not None and val > maxVal) or \
               (minVal is not None and val < minVal):
                raise WLParamRangeError("Parameter value %d is outside range [%d...%d]" % (val,
                                                                                           minVal,
                                                                                           maxVal))  
            
            return val
        elif isinstance(wlp, TSWLParamFloat):
            return float(value)
        elif isinstance(wlp, TSWLParamTime):
            return parseTimeParam(value)
        elif isinstance(wlp, TSWLParamString):
            val = str(value)
            
            maxLen = WLParamHelper.getStringLength(wlp)
            
            if maxLen is not None and len(val) > maxLen:
                raise WLParamRangeError("String too long (maximum: %d, current: %d)" % (maxLen, 
                                                                                        len(val)))
            
            return val
        elif isinstance(wlp, TSWLParamStringSet):
            return str(value)
        
        return value
            