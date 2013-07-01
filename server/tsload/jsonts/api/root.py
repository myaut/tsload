'''
Created on 01.06.2013

@author: myaut
'''

from tsload.jsonts.object import TSObject as tso
from tsload.jsonts.api import TSAgentInterface, TSMethod

class TSHelloResponse(tso):
    agentId = tso.Int()
    
class TSClientDescriptor(tso):    
    id = tso.Int()
    type = tso.String()
    uuid = tso.String()
    authType = tso.Int()
    
    # See JSONTS.STATE_*
    state = tso.Int()
    
    endpoint = tso.String()

class RootAgent(TSAgentInterface):
    hello = TSMethod(tso.Object(TSHelloResponse),
                     agentType = tso.String(),
                     agentUuid = tso.String())
    
    authMasterKey = TSMethod(masterKey = tso.String())
    
    listClients = TSMethod(tso.Array(tso.Object(TSClientDescriptor)))
