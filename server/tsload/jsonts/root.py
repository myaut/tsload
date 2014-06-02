
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



from tsload import logging

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
        
        self.logger = logging.getLogger('RootAgent')
        
        for command in ['hello', 'authMasterKey', 'listClients']:
            self.server.listenerFlows.append(Flow(dstAgentId = rootAgentId, 
                                                  command = command))
    
    @TSMethodImpl(RootAgent.hello)
    def hello(self, context, agentType, agentUuid):
        client = context.client
        agentId = client.getId()
        
        self.logger.info('Connected client %s from %s', client, client.endpointStr)
        self.logger.info('\t\ttype: %s uuid: %s', agentType, agentUuid)
        
        client.setAgentInfo(agentType, agentUuid)
        
        self.server.notifyAgentRegister(client)
        
        result = TSHelloResponse()
        result.agentId = agentId
        return result
    
    @TSMethodImpl(RootAgent.authMasterKey)
    def authMasterKey(self, context, masterKey):
        client = context.client
        
        self.logger.info('Authorizing client %s with master key', client)
        
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
