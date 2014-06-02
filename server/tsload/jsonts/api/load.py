
'''
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
'''  



from tsload.jsonts.object import TSObject as tso
from tsload.jsonts.api import TSAgentInterface, TSMethod

class TSAgentDescriptor(tso):    
    hostname = tso.String()
    
    domainname = tso.String()
    osname = tso.String()
    release = tso.String()
    machineArch = tso.String()
    
    numCPUs = tso.Int()
    numCores = tso.Int()
    memTotal = tso.Int()
    
class TSWLParamCommon(tso):
    WLPF_NO_FLAGS = 0x00
    WLPF_OPTIONAL = 0x01
    
    flags = tso.Int()
    description = tso.String()
    
class TSWLParamBoolean(TSWLParamCommon):
    default = tso.Optional(tso.Bool())

class TSWLParamInteger(TSWLParamCommon):
    default = tso.Optional(tso.Int())
    min = tso.Optional(tso.Int())
    max = tso.Optional(tso.Int())
class TSWLParamSize(TSWLParamInteger): pass
class TSWLParamTime(TSWLParamInteger): pass

class TSWLParamFloat(TSWLParamCommon):
    default = tso.Optional(tso.Float())
    min = tso.Optional(tso.Float())
    max = tso.Optional(tso.Float())

class TSWLParamString(TSWLParamCommon):
    default = tso.Optional(tso.String())
    len = tso.Int()
class TSWLParamFilePath(TSWLParamString): pass
class TSWLParamDisk(TSWLParamString): pass
class TSWLParamCPUObject(TSWLParamString): pass

class TSWLParamStringSet(TSWLParamCommon):
    strset = tso.Array(tso.String())
    default = tso.Optional(tso.String())

# See agent/lib/libtsload/wlparam.c -> wlt_type_names
tsWLParamClassMap = {
    # null, - Unresolvable
    "bool": TSWLParamBoolean,
    "integer": TSWLParamInteger,
    "float": TSWLParamFloat,
    "string": TSWLParamString,
    "strset": TSWLParamStringSet,

    # metatypes
    "size": TSWLParamSize,
    "time": TSWLParamTime,
    "filepath": TSWLParamFilePath,
    "cpuobject": TSWLParamCPUObject,
    "disk": TSWLParamDisk
}

TSWorkloadParameter = tso.MultiObject('type', tsWLParamClassMap)

class TSWorkloadType(tso):
    module = tso.String()
    path = tso.String()
    
    wlclass = tso.Array(tso.String())
    
    params = tso.Map(TSWorkloadParameter)

class TSResource(tso):
    children = tso.Array(tso.String())
    type = tso.String()
    data = tso.Any()

class LoadAgent(TSAgentInterface):
    getHostInfo = TSMethod(tso.Object(TSAgentDescriptor))
    
    getWorkloadTypes = TSMethod(tso.Map(TSWorkloadType))
    getResources = TSMethod(tso.Map(tso.Map(TSResource)))
    
