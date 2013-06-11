'''
Experiment service agent
'''

from tsload.jsonts import Flow
from tsload.jsonts.server import TSLocalAgent, AgentListener

from tsload.jsonts.api import TSMethodImpl
from tsload.jsonts.api.expsvc import *
from tsload.jsonts.api.load import *

from storm.locals import create_database
from storm.store import Store

from twisted.internet.defer import inlineCallbacks

expsvcAgentUUID = '{8390b21d-3abb-4de6-a3df-0ccd164908ee}'
expsvcAgentType = 'expsvc'

expsvcAgentId = 2

class TSExperimentSvcAgent(TSLocalAgent):
    agentId = expsvcAgentId
    
    uuid = expsvcAgentUUID
    agentType = expsvcAgentType
    
    def __init__(self, server, connString):
        TSLocalAgent.__init__(self, server)
        
        self.rootAgent = server.localAgents[0]
        
        self.database = create_database(connString)
        self.dbStore = Store(self.database)
        
        self.server.listenerAgents.append(AgentListener('load',
                                                        self.onAgentRegister,
                                                        self.onAgentDisconnect))
        self.loadAgents = {}
    
    @inlineCallbacks
    def onAgentRegister(self, client):
        agentId = client.getId()
        agent = self.createRemoteAgent(agentId, LoadAgent)
        
        hostInfo = yield agent.getHostInfo()
        
        print hostInfo.hostname
    
    def onAgentDisconnect(self, client):
        pass
    