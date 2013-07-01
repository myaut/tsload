'''
Experiment service agent
'''

import uuid
from datetime import datetime
import time

from storm.locals import create_database
from storm.store import Store

from tsload.expsvc.model import *
from tsload.expsvc.agent import LoadAgentInfo

from tsload.jsonts import Flow, JSONTS, datetimeToTSTime
from tsload.jsonts.api import TSMethodImpl
from tsload.jsonts.api.expsvc import *
from tsload.jsonts.api.load import *

from tsload.jsonts.server import TSLocalAgent, AgentListener

from twisted.internet.defer import inlineCallbacks, returnValue

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
    def registerNewAgent(self, client, agent):
        hostInfo = yield agent.getHostInfo()
        
        agentObj = Agent()
        agentObj.uuid = uuid.UUID(client.agentUuid)
        agentObj.agentType = 'load'
        
        # FIXME: Sometimes domainname passed with hostname
        agentObj.hostname = unicode(hostInfo.hostname)
        agentObj.domainname = unicode(hostInfo.domainname)
        agentObj.osname = hostInfo.osname
        agentObj.release = hostInfo.release
        agentObj.machineArch = hostInfo.machineArch
        
        agentObj.numCPUs = hostInfo.numCPUs
        agentObj.numCores = hostInfo.numCores
        agentObj.memTotal = hostInfo.memTotal
        
        agentObj.lastOnline = datetime.now()
        
        yield self.dbStore.add(agentObj)
        yield self.dbStore.commit()
        
        returnValue(agentObj)
    
    @inlineCallbacks
    def onAgentRegister(self, client):
        if client.agentType == 'load':        
            if client.agentUuid in self.loadAgents:
                raise JSONTS.Error(JSONTS.AE_INVALID_STATE, 
                                   "Loader agent with uuid '%s' already registered" % client.agentUuid)
            
            agentId = client.getId()
            agent = self.createRemoteAgent(agentId, LoadAgent)
            
            agentSet = yield self.dbStore.find(Agent,
                                               Agent.uuid == uuid.UUID(client.agentUuid))
            agentObj = yield agentSet.one()
            
            if agentObj is None:
                agentObj = yield self.registerNewAgent(client, agent)
            else:
                # Update last online timestamp
                agentObj.lastOnline = datetime.now()
                
                yield self.dbStore.add(agentObj)
                yield self.dbStore.commit()
            
            agentInfo = LoadAgentInfo(agent, agentObj)
            
            self.loadAgents[client.agentUuid] = agentInfo
            
            print "Registered agent %s with uuid '%s'" % (agentObj.hostname, 
                                                          client.agentUuid)
                  
    def onAgentDisconnect(self, client):
        if client.agentType == 'load':
            agentInfo = self.loadAgents[client.agentUuid]
            print 'Disconnected agent %s' % agentInfo.agentObj.hostname
            
            del self.loadAgents[client.agentUuid]
    
    @TSMethodImpl(ExpSvcAgent.listAgents)
    @inlineCallbacks
    def listAgents(self, context):
        agentsList = {}
        
        agentSet = yield self.dbStore.find(Agent)
        
        for agentObj in agentSet:
            # TODO: should filter agents according to users ACL
            
            agentUuid = str(agentObj.uuid)
            descriptor = TSExpSvcAgentDescriptor()
            
            descriptor.agentId = agentObj.id
            descriptor.lastOnline = datetimeToTSTime(agentObj.lastOnline)
            
            descriptor.isOnline = agentUuid in self.loadAgents
            
            for field in ('hostname', 'domainname', 'osname', 'release',
                          'machineArch', 'numCPUs', 'numCores', 'memTotal'):
                setattr(descriptor, field, getattr(agentObj, field))
                
            agentsList[agentUuid] = descriptor
            
        returnValue(agentsList)
            
        
    