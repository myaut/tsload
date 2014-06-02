
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



from tsload.jsonts.object import TSObject as tso
from tsload.jsonts.api import TSAgentInterface, TSMethod

from tsload.jsonts.api.load import TSAgentDescriptor, TSWorkloadType

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

class ResourceAllocationMode:
    R_DEDICATED         = 0
    R_SHARED_EXPERIMENT = 1
    R_SHARED_ANYONE     = 2

class TSExperimentThreadPool(tso):
    agentId = tso.Nullable(tso.Int())
    numWorkers = tso.Int()
    
    @staticmethod
    def createEmptyThreadPool():
        threadpool = TSExperimentThreadPool()
        
        threadpool.agentId = None
        threadpool.numWorkers = 0
        
        return threadpool

class TSExperimentWorkload(tso):
    agentId = tso.Nullable(tso.Int())
    workloadType = tso.Nullable(tso.String())
    
    threadpool = tso.Nullable(tso.String())
    params = tso.Any()
    
    @staticmethod
    def createEmptyWorkload():
        workload = TSExperimentWorkload()
        
        workload.agentId = None
        workload.workloadType = None
        workload.threadpool = None
        workload.params = {}
        
        return workload

class TSExperimentProfileInfo(tso):
    profileId = tso.Int()
    
    userId = tso.Int()
    
    description = tso.String()
    creationDate = tso.Int()
    
class TSExperimentProfile(TSExperimentProfileInfo):    
    threadpools = tso.Map(tso.Object(TSExperimentThreadPool))
    workloads = tso.Map(tso.Object(TSExperimentWorkload))

    @staticmethod
    def createEmptyProfile():
        profile = TSExperimentProfile()
        
        profile.profileId = -1
        
        profile.threadpools = {}
        profile.workloads = {}
        
        profile.description = ""
        
        profile.userId = -1
        profile.creationDate = -1
        
        return profile

class ExpSvcAgent(TSAgentInterface):
    listAgents = TSMethod(tso.Map(tso.Object(TSExpSvcAgentDescriptor)))
    
    getWorkloadTypes = TSMethod(tso.Map(tso.Object(TSWorkloadType)),
                                agentId = tso.Int())
    getAgentResources = TSMethod(tso.Object(TSAgentResourceInfo),
                                 agentId = tso.Int())
    
    listProfiles = TSMethod(tso.Map(tso.Object(TSExperimentProfileInfo)))
    
    getProfile = TSMethod(tso.Object(TSExperimentProfile),
                          profileName = tso.String(),
                          profile = tso.Object(TSExperimentProfileInfo))
    
    configureProfile = TSMethod(profileName = tso.String(),
                                profile = tso.Object(TSExperimentProfile))
