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

class ExpSvcAgent(TSAgentInterface):
    listAgents = TSMethod(tso.Map(tso.Object(TSExpSvcAgentDescriptor)))