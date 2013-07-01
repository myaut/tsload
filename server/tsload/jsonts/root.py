'''
Created on May 13, 2013

@author: myaut
'''

from tsload.jsonts import JSONTS, Flow

from tsload.jsonts.server import TSLocalAgent, TSServerClient

from tsload.jsonts.api import TSMethodImpl
from tsload.jsonts.api.root import RootAgent, TSHelloResponse, TSClientDescriptor

rootAgentUUID = '{14f498da-a689-4341-8869-e4a292b143b6}'
rootAgentType = 'root'

rootAgentId = 0

class TSRootAgent(TSLocalAgent):
    agentId = rootAgentId
    
    uuid = rootAgentUUID
    agentType = rootAgentType
    
    def __init__(self, server):
        TSLocalAgent.__init__(self, server)
        
        for command in ['hello', 'authMasterKey', 'listClients']:
            self.server.listenerFlows.append(Flow(dstAgentId = rootAgentId, 
                                                  command = command))
    
    @TSMethodImpl(RootAgent.hello)
    def hello(self, context, agentType, agentUuid):
        client = context.client
        agentId = client.getId()
        
        client.setAgentInfo(agentType, agentUuid)
        
        self.server.notifyAgentRegister(client)
        
        result = TSHelloResponse()
        result.agentId = agentId
        return result
    
    @TSMethodImpl(RootAgent.authMasterKey)
    def authMasterKey(self, context, masterKey):
        client = context.client
        
        self.server.doTrace('Authentificating client %s with key %s', client, masterKey)
        
        if str(self.server.masterKey) == masterKey:
            client.authorize(TSServerClient.AUTH_MASTER)
            return
        
        raise JSONTS.Error(JSONTS.AE_INVALID_DATA, 'Master key invalid')
    
    @TSMethodImpl(RootAgent.listClients)
    def listClients(self, context):
        clients = []
        
        for agentId in self.server.clients:
            descr = TSClientDescriptor()
            client = self.server.clients[agentId]
            
            descr.id = agentId
            descr.type = client.agentType
            descr.uuid = client.agentUuid
            descr.state = client.state
            descr.endpoint = client.endpointStr
            descr.authType = client.auth
            
            clients.append(descr)
            
        return clients