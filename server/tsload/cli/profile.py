
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



import time

from tsload.jsonts import JSONTS, tstimeToUnixTime
from tsload.jsonts.api.expsvc import *
from tsload.jsonts.object import TSObject as tso

from tsload.cli.agent import agentSelector
from tsload.cli.context import CLIContext, NextContext, ReturnContext, SameContext

from tsload.wlparam import WLParamHelper

from twisted.internet.defer import inlineCallbacks, returnValue

class ProfileError(Exception):
    pass

class ThreadpoolConfigurationContext(CLIContext):
    operations = ['info', 'set']
    
    @SameContext()
    def info(self, args):
        print 'Threadpool', self.name
        print '\tAgent id:', self.threadpool.agentId
        print '\tNumber of workers:', self.threadpool.numWorkers
    
    @SameContext()
    @inlineCallbacks
    def set(self, args):
        paramName  = args.pop(0)
        paramValue = args.pop(0) 
        
        if paramName == 'agent':
            agentList = yield self.cli.expsvcAgent.listAgents()
            _, agent = agentSelector(agentList, paramValue)
            
            if agent is None:
                raise ProfileError('No such agent found!')
            
            self.threadpool.agentId = agent.agentId
        elif paramName == 'workers':
            numWorkers = int(paramValue)
            
            if numWorkers < 0:
                raise ProfileError('Number of threadpool workers must not be negative!')
            
            self.threadpool.numWorkers = numWorkers
        else:
            raise ProfileError("Invalid parameter '%s'" % paramName)
        
        # We didn't raise ProfileError if we are here - set profile modified flag
        self.parent.setCommitState(False)
        
    set.args = ['{agent|workers} <value>']
        

class WorkloadConfigurationContext(CLIContext):
    operations = ['info', 'set']
    
    workloadType = None
    workloadTypeName = ''
    
    @SameContext()
    def info(self, args):
        print 'Workload', self.name
        print '\tAgent id:', self.workload.agentId
        print '\tWorkload type:', self.workload.workloadType 
        print '\tThreadpool:', self.workload.threadpool
        print '\tParams:'
        
        for paramName, paramValue in self.workload.params.iteritems():
            print '\t\twl:%s = %s' % (paramName, paramValue) 
    
    @SameContext()
    @inlineCallbacks
    def set(self, args):
        paramName  = args.pop(0)
        paramValue = args.pop(0) 
        
        if paramName == 'agent':
            agentList = yield self.cli.expsvcAgent.listAgents()
            _, agent = agentSelector(agentList, paramValue)
            
            if agent is None:
                raise ProfileError('No such agent found!')
            
            self.workload.agentId = agent.agentId
        elif paramName == 'wltype' or paramName == 'wlt':
            if self.workload.agentId is None:
                raise ProfileError("You didn't select agent, could not set workload type.")
            
            wltList = yield self.cli.expsvcAgent.getWorkloadTypes(agentId = self.workload.agentId)
            
            if paramValue not in wltList:
                raise ProfileError("Unknown workload type.")
            
            self.workload.workloadType = paramValue
            self.workloadType = wltList[paramValue]
            
            self.workload.params = {}
            for paramName, param in self.workloadType.params.iteritems():
                defaultValue = WLParamHelper.getDefaultValue(param)
                self.workload.params[paramName] = defaultValue 
        elif paramName == 'threadpool' or paramName == 'tp':
            self.workload.threadpool = paramValue
        elif paramName.startswith('wl:'):
            paramName = paramName[3:]
            
            if paramName not in self.workload.params:
                raise ProfileError("Invalid workload parameter '%s'." % paramName)
            
            wlp = self.workloadType.params[paramName]
            value = WLParamHelper.parseParamValue(wlp, paramValue)
            
            self.workload.params[paramName] = value
        else:
            raise ProfileError("Invalid parameter '%s'" % paramName) 
        
        # We didn't raise ProfileError if we are here - set profile modified flag
        self.parent.setCommitState(False)
    set.args = ['{agent|wltype|threadpool|wl:*} <value>']
            

class ProfileConfigurationContext(CLIContext):
    operations = ['info', 'commit', 'create', 'select', 'remove']
    
    def _createThreadpoolContext(self, threadpoolName, threadpool = None):
        threadpoolContext = self.nextContext(ThreadpoolConfigurationContext)
        
        if threadpool is None:
            threadpool = TSExperimentThreadPool.createEmptyThreadPool()
            threadpool.name = threadpoolName
        
        threadpoolContext.threadpool = threadpool 
        threadpoolContext.name = repr(threadpoolName)
        
        self.threadpoolContexts[threadpoolName] = threadpoolContext
        
        return threadpoolContext, threadpool
    
    def _createWorkloadContext(self, workloadName, workload = None):
        workloadContext = self.nextContext(WorkloadConfigurationContext)
            
        if workload is None:
            workload = TSExperimentWorkload.createEmptyWorkload()
            workload.name = workloadName
        
        workloadContext.workload = workload 
        workloadContext.name = repr(workloadName)
        
        self.workloadContexts[workloadName] = workloadContext
        
        return workloadContext, workload
    
    def __init__(self, parent, parentCli, profile, profileName):
        CLIContext.__init__(self, parent, parentCli)
        
        self.threadpoolContexts = {}
        self.workloadContexts = {}
        
        for threadpoolName, threadpool in profile.threadpools.iteritems():
            self._createThreadpoolContext(threadpoolName, threadpool)
        
        for workloadName, workload in profile.workloads.iteritems():
            self._createWorkloadContext(workloadName, workload)
        
        self.profile = profile
        self.profileName = profileName
        
        self.setCommitState(True)
    
    def setCommitState(self, state):
        self.commitState = state
        
        self.name = repr(self.profileName)
        if not state:
            self.name += '*'
    
    @SameContext()
    def info(self, args):
        print self.profileName
        
        creationDate = time.ctime(tstimeToUnixTime(self.profile.creationDate))
        
        print '\t' + self.profile.description
        print '\tID:', self.profile.profileId
        print '\tUser ID:', self.profile.userId
        print '\tCreation date:', creationDate
        
        print '\tThreadpools:'
        for threadpoolName in self.profile.threadpools:
            print '\t\t' + threadpoolName
        
        print '\tWorkloads:'
        for workloadName in self.profile.workloads:
            print '\t\t' + workloadName
            
        return args
    
    def _argsHelper(self, args):
        objType = args.pop(0)
        objName = args.pop(0)
        
        tThreadpool = objType == 'threadpool' or objType == 'tp'
        tWorkload = objType == 'workload' or objType == 'wl'
        
        return tThreadpool, tWorkload, objName
    
    def create(self, args):
        tThreadpool, tWorkload, objName = self._argsHelper(args)
        
        self.setCommitState(False)
        
        if tThreadpool:
            threadpoolContext, threadpool = self._createThreadpoolContext(objName)
            
            self.profile.threadpools[objName] = threadpool
            
            return threadpoolContext, args
        elif tWorkload:
            workloadContext, workload = self._createWorkloadContext(objName)
            
            self.profile.workloads[objName] = workload
            
            return workloadContext, args
        
        raise ProfileError('Invalid object type')
        
    def select(self, args):
        tThreadpool, tWorkload, objName = self._argsHelper(args)
        
        if tThreadpool:
            return self.threadpoolContexts[objName], args
        elif tWorkload:
            return self.workloadContexts[objName], args
        
        raise ProfileError('Invalid object type')
    
    @SameContext()
    def remove(self, args):
        tThreadpool, tWorkload, objName = self._argsHelper(args)
        
        self.setCommitState(False)
        
        if tThreadpool:
            del self.threadpoolContexts[objName]
            del self.profile.threadpools[objName]
            return args
        elif tWorkload:
            del self.workloadContexts[objName]
            del self.profile.workloads[objName]
            return args
        
        raise ProfileError('Invalid object type')
    
    @SameContext()
    @inlineCallbacks
    def commit(self, args):
        yield self.cli.expsvcAgent.configureProfile(profileName = self.profileName,
                                                    profile = self.profile)
        
        self.setCommitState(True)
        
        print 'Experiment profile saved'

class ProfileSelectContext(CLIContext):
    name = 'select'
    async = True
    
    def doResponse(self, args):
        profileName, profile = args
        
        config = self.parent.nextContext(ProfileConfigurationContext, profile, profileName)
        
        return config, args

class ProfileRootContext(CLIContext):
    name = 'profile'
    operations = ['list', 'create', 'select']
    
    @SameContext()
    @inlineCallbacks
    def list(self, args):
        profiles = yield self.cli.expsvcAgent.listProfiles()
        
        fmtstr = '%-4s %-3s %-8s %-24s %s'
        
        print fmtstr % ('ID', 'USERID', 'NAME', 'CREATED', 'DESCRIPTION')
        
        for profileName, profile in profiles.iteritems():
            creationDate = time.ctime(tstimeToUnixTime(profile.creationDate))
            
            print fmtstr % (profile.profileId,
                            profile.userId,
                            profileName,
                            creationDate,
                            profile.description)
    
    def create(self, args):
        profileName = args.pop()
        profile = TSExperimentProfile.createEmptyProfile()
        
        config = self.nextContext(ProfileConfigurationContext, profile, profileName)
        config.setCommitState(False)
        
        return config, args
    create.args = ['<name>']
    
    @NextContext(ProfileSelectContext)
    @inlineCallbacks
    def select(self, args):
        profileName = args.pop()
        
        profiles = yield self.cli.expsvcAgent.listProfiles()
        
        if profileName not in profiles:
            raise ProfileError('No such profile found: %s' % profileName)
        
        profile = profiles[profileName]
        profile = yield self.cli.expsvcAgent.getProfile(profileName = profileName, 
                                                        profile = profile)
        
        returnValue((profileName, profile))
        
    select.args = ['<name>']
