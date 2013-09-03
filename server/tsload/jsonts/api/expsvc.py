'''
Created on Jun 11, 2013

@author: myaut
'''

from tsload.jsonts.object import TSObject as tso
from tsload.jsonts.api import TSAgentInterface, TSMethod

from tsload.jsonts.api.load import TSAgentDescriptor

class TSExpSvcAgentDescriptor(TSAgentDescriptor):
    agentId = tso.Int()
    
    isOnline = tso.Bool()
    lastOnline = tso.Int()
    
class TSAgentResource(tso):
    resId = tso.Int()
    
    resClass = tso.String()
    resType = tso.String()
    
    data = tso.Any()
    
    state = tso.Int()

class TSAgentResourceChild(tso):
    parentName = tso.String()
    childName = tso.String()

class TSAgentResourceInfo(tso):
    resources = tso.Map(TSAgentResource)
    childLinks = tso.Array(TSAgentResourceChild)

class ResourceState:
    R_ONLINE   = 0
    R_OFFLINE  = 1
    R_BUSY     = 2
    R_DETACHED = 3

class ExpSvcAgent(TSAgentInterface):
    listAgents = TSMethod(tso.Map(tso.Object(TSExpSvcAgentDescriptor)))
    
    getAgentResources = TSMethod(tso.Object(TSAgentResourceInfo),
                                 agentId = tso.Int())