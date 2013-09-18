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
from tsload.expsvc.resmgr import ResourceManager

from tsload.jsonts import Flow, JSONTS, datetimeToTSTime
from tsload.jsonts.api import TSMethodImpl
from tsload.jsonts.api.expsvc import *
from tsload.jsonts.api.load import *

from tsload.jsonts.server import TSLocalAgent, AgentListener

from twisted.internet import reactor
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
        
        self.resourceManager = ResourceManager(self.dbStore) 
    
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
            
            reactor.callLater(0.0, self.fetchWorkloadTypes, agent, agentObj)
            reactor.callLater(0.1, self.resourceManager.registerLoadAgent, agent, agentObj)
                  
    def onAgentDisconnect(self, client):
        if client.agentType == 'load':
            yield self.resourceManager.unregisterLoadAgent(agentInfo.agentObj)
            
            agentInfo = self.loadAgents[client.agentUuid]
            print 'Disconnected agent %s' % agentInfo.agentObj.hostname
            
            del self.loadAgents[client.agentUuid]
    
    @inlineCallbacks
    def fetchWorkloadTypes(self, agent, agentObj):
        wltypeList = yield agent.getWorkloadTypes()
        
        wltSet = yield self.dbStore.find(WorkloadType,
                                         WorkloadType.agent == agentObj)
        
        for wltypeName, wltype in wltypeList.iteritems():
            wltObj = wltSet.find(WorkloadType.name == wltypeName)
            wltObj = wltObj.any()
            
            if wltObj is None:
                wltObj = WorkloadType()
                
                wltObj.agent = agentObj
                
                wltObj.name = wltypeName
                wltObj.module = wltype.module
                wltObj.modulePath = wltype.path
                
                wltObj.classList = ','.join(wltype.wlclass)
                
                yield self.dbStore.add(wltObj) 
            
            paramSet = yield self.dbStore.find(WorkloadParam,
                                               WorkloadParam.workloadType == wltObj)
            
            # Update parameter list
            for paramObj in paramSet:
                if paramObj.name not in wltype.params:
                    paramObj.remove()
                    continue
                
                paramObj.data = wltype.params.serialize()
                # Remove serialized object from params array
                del wltype.params[paramObj.name]
                
                yield self.dbStore.add(paramObj)
            
            for paramName, param in wltype.params.iteritems():
                paramObj = WorkloadParam()
                
                paramObj.name = paramName
                paramObj.workloadType = wltObj
                paramObj.paramData = TSWorkloadParameter.serialize(param)
                
                yield self.dbStore.add(paramObj)
                
        yield self.dbStore.commit()
    
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
    
    @TSMethodImpl(ExpSvcAgent.getWorkloadTypes)
    @inlineCallbacks
    def getWorkloadTypes(self, context, agentId):
        wltSet = yield self.dbStore.find(WorkloadType,
                                         WorkloadType.aid == agentId)
        
        paramsQuery = (WorkloadParam,
                       Join(WorkloadType,
                            And(WorkloadType.aid == agentId,
                                WorkloadParam.wltid == WorkloadType.id)))
        paramsGlobalSet = yield self.dbStore.using(*paramsQuery).find(WorkloadParam)
        
        wltypeList = {}
        
        for wltObj in wltSet:
            paramsSet = yield paramsGlobalSet.find(WorkloadParam.workloadType == wltObj)
            
            wltype = TSWorkloadType()
            
            wltype.module = wltObj.module
            wltype.path = wltObj.modulePath
            
            wltype.wlclass = wltObj.classList.split(',')
            
            wltype.params = {}
            
            for paramObj in paramsSet:
                param = TSWorkloadParameter.deserialize(paramObj.paramData)
                
                wltype.params[paramObj.name] = param
            
            wltypeList[wltObj.name] = wltype
        
        returnValue(wltypeList)
    
    @TSMethodImpl(ExpSvcAgent.getAgentResources)
    @inlineCallbacks
    def getAgentResources(self, context, agentId):
        resourceInfo = yield self.resourceManager.getAgentResources(agentId)
        
        returnValue(resourceInfo)
    