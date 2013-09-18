import json

from storm.locals import *

from twisted.internet.defer import inlineCallbacks, returnValue

from tsload import logging

from tsload.jsonts.api.expsvc import ResourceState, TSAgentResource, TSAgentResourceChild, TSAgentResourceInfo

from tsload.expsvc.model import AgentResource, AgentResourceChild

class ResourceManager:
    def __init__(self, dbStore):
        self.dbStore = dbStore
        
        self.logger = logging.getLogger('ResMgr')
    
    @inlineCallbacks
    def registerLoadAgent(self, agent, agentObj):
        resources = yield agent.getResources()
        
        resObjects = {}
        
        resSet = yield self.dbStore.find(AgentResource,
                                         AgentResource.agent == agentObj)
        
        # FIXME: Should detach resources that didn't reported
        # FIXME: Should alter resource data if it was changed
        
        for subsysName, subsysResources in resources.items():
            for resName, res in subsysResources.items():
                resObj = resSet.find(AgentResource.name == resName)
                resObj = resObj.any()
                
                if resObj is None:
                    resObj = AgentResource()
                    
                    resObj.agent = agentObj
                    resObj.resourceClass = subsysName
                    
                    resObj.name = resName
                    resObj.resourceType = res.type
                    
                    resObj.data = res.data
                    
                resObj.state = ResourceState.R_ONLINE
                
                yield self.dbStore.add(resObj)
                
                resObjects[resName] = resObj
                
                self.logger.info('Registered resource %s from subsystem %s for agent #%d', 
                                 resName, subsysName, agentObj.id)
        
        yield self.dbStore.commit()
        
        for subsysName, subsysResources in resources.items():
            for resName, res in subsysResources.items():
                for child in res.children:                    
                    parentResObj = resObjects.get(resName)
                    childResObj = resObjects.get(child)
                    
                    if parentResObj is not None and childResObj is not None:
                        childObj = AgentResourceChild()
                        
                        childObj.pid = parentResObj.id
                        childObj.cid = childResObj.id
                        
                        # Duplicates are checked over database constraints
                        yield self.dbStore.add(childObj)
        
        yield self.dbStore.commit()
    
    @inlineCallbacks
    def unregisterLoadAgent(self, agentObj):
        resSet = yield self.dbStore.find(AgentResource,
                                         AgentResource.agent == agentObj)
        
        for resObj in resSet:
            if resObj.state in [ResourceState.R_ONLINE,
                                ResourceState.R_BUSY]:
                resObj.state = ResourceState.R_OFFLINE
                
                yield self.dbStore.add(resObj)
        
        yield self.dbStore.commit()
    
    @inlineCallbacks
    def getAgentResources(self, agentId):
        info = TSAgentResourceInfo()
        
        info.resources = {}
        info.childLinks = []
        
        # FIXME: Check if agentId really exists
        
        resSet = yield self.dbStore.find(AgentResource,
                                         AgentResource.aid == agentId)
        
        resMap = {}
        
        for resObj in resSet:
            resource = TSAgentResource()
            
            resource.resId = resObj.id
            
            resource.resClass = resObj.resourceClass
            resource.resType = resObj.resourceType
            
            resource.state = resObj.state
            
            resource.data = resObj.data
            
            info.resources[resObj.name] = resource
            
            resMap[resObj.id] = resObj.name
        
        resChildOrigin = (AgentResourceChild,
                          Join(AgentResource,
                               And(AgentResource.aid == agentId,
                                   AgentResource.id == AgentResourceChild.pid)))
        
        resChildSet = yield self.dbStore.using(*resChildOrigin).find(AgentResourceChild)
        
        for resChildObj in resChildSet:
            childLink = TSAgentResourceChild()
            
            childLink.parentName = resMap[resChildObj.pid]
            childLink.childName = resMap[resChildObj.cid]
            
            info.childLinks.append(childLink)
        
        returnValue(info)
                
                    
                
                
        