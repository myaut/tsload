'''
Created on Jun 11, 2013

@author: myaut
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
    
class LoadAgent(TSAgentInterface):
    getHostInfo = TSMethod(tso.Object(TSAgentDescriptor))