'''
Created on May 13, 2013

@author: myaut
'''

from tsload.jsonts import JSONTS, Flow
from tsload.jsonts.agent import TSAgent
from tsload.jsonts.server import TSServerClient

rootAgentUUID = '{14f498da-a689-4341-8869-e4a292b143b6}'
rootAgentType = 'root'

rootAgentId = 0

class TSRootAgent(TSAgent):
    uuid = rootAgentUUID
    agentType = rootAgentType
    
    def __init__(self, server):
        self.server = server
        
        self.client = TSServerClient(server, rootAgentId)
        self.client.setAgentInfo(self.agentType, self.uuid)
        
        for command in ['hello', 'authMasterKey', 'listClients']:
            self.server.listenerFlows.append(Flow(dstAgentId = rootAgentId, 
                                                  command = command))
    
    def hello(self, context, agentType, agentUuid):
        agentId = context.client.getId()
        client = self.server.clients[agentId]
        
        context.client.setAgentInfo(agentType, agentUuid)
        
        return {'agentId': context.client.getId()}
    
    def authMasterKey(self, context, masterKey):
        client = context.client
        
        self.server.doTrace('Authentificating client %s with key %s', client, masterKey)
        
        if str(self.server.masterKey) == masterKey:
            client.authorize(TSServerClient.AUTH_MASTER)
            return {}
        
        raise JSONTS.Error(JSONTS.AE_INVALID_DATA, 'Master key invalid')
    
    def listClients(self, context):
        clients = {}
        
        for agentId in self.server.clients:
            client = self.server.clients[agentId]
            clients[agentId] = client.serializeAgentInfo()
            
        return clients