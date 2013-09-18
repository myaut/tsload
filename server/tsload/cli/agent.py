'''
Created on 06.05.2013

@author: myaut
'''

import sys
import time

from tsload.jsonts import JSONTS, tstimeToUnixTime
from tsload.cli.context import CLIContext, NextContext, ReturnContext, SameContext
from tsload.jsonts.api.load import *
from tsload.jsonts.api.expsvc import ResourceState
from tsload.wlparam import WLParamHelper

from twisted.internet.defer import inlineCallbacks, returnValue

class AgentContext(CLIContext):
    def setAgentId(self, agentId):
        self.agentId = agentId
        self.name = str(agentId)

class LoadAgentListContext(CLIContext):
    name = 'list'
    async = True
    
    @ReturnContext()
    def doResponse(self, agentsList):
        fmtstr = '%-4s %-12s %-38s %24s'
        
        print fmtstr % ('ID', 'HOSTNAME', 'UUID', 'LAST ONLINE')
        
        for agentUuid, agent in agentsList.iteritems():
            agentId = str(agent.agentId) 
            agentId += '*' if agent.isOnline else ''
            
            lastOnline = time.ctime(tstimeToUnixTime(agent.lastOnline))
            
            print fmtstr % (agentId, agent.hostname, agentUuid, lastOnline)
            
            print '\tOS name:', agent.osname
            print '\tOS release:', agent.release
            print '\tMachine architecture:', agent.machineArch
            
            print '\tNumber of CPUs:', agent.numCPUs
            print '\tNumber of cores:', agent.numCores        
            print '\tTotal amount of memory:', agent.memTotal


class LoadAgentContext(CLIContext):
    operations = ['wltype', 'resources']
    
    def setupAgent(self, loadAgentId, isOnline):
        self.loadAgentId = loadAgentId
        self.isOnline = isOnline
    
    @SameContext()
    @inlineCallbacks
    def wltype(self, args):
        wltypeList = yield self.cli.expsvcAgent.getWorkloadTypes(agentId = self.loadAgentId)
        
        if args:
            wltypeName = args.pop()
            
            try:
                wltype = wltypeList[wltypeName]
                
                print 'Workload type:', wltypeName
                print 'Workload classes:', ' '.join(wltype.wlclass)
                print 'Module name:', wltype.module
                print 'Module path:', wltype.wlclass
                
                print 'Params:'
                
                fmtstr = '%-12s %-12s %-12s %-8s %s'
                
                print fmtstr % ('NAME', 'TYPE', 'RANGE', 
                                'DEFAULT', 'DESCRIPTION')
                                
                for paramName, param in wltype.params.iteritems():
                    if not param.flags & TSWLParamCommon.WLPF_OPTIONAL:
                        paramName += '*'
                    
                    wlpType = WLParamHelper.getTypeName(param)
                    
                    minVal, maxVal = WLParamHelper.getIntegerRange(param)
                    lenVal = WLParamHelper.getStringLength(param)
                    range = 'None'
                    
                    if minVal is not None and maxVal is not None:
                        range = '[%s...%s]' % (minVal, maxVal)
                    elif lenVal is not None:
                        range = 'len: %s' % lenVal
                    
                    default = WLParamHelper.getDefaultValue(param)
                    
                    print fmtstr % (paramName, wlpType, range, default, param.description)                
            except KeyError:
                print 'ERROR: No such workload type "%s"' % wltypeName
        else:
            # Print list of all available workload types
            fmtstr = '%-12s %-16s %s'
             
            print fmtstr % ('MODULE', 'WORKLOAD', 'CLASSES')
            
            for wltypeName, wltype in wltypeList.iteritems():
                print fmtstr % (wltype.module,
                                wltypeName,
                                ' '.join(wltype.wlclass))
        
    wltype.args = ["[name]"]
    
    @SameContext()
    @inlineCallbacks
    def resources(self, args):
        resInfo = yield self.cli.expsvcAgent.getAgentResources(agentId = self.loadAgentId)
        
        resList = resInfo.resources
        
        if args:
            resName = args.pop()
            
            if resName[0] == '.':
                resClass = resName[1:]
                
                if '.' in resClass:
                    resClass, resType = resClass.split('.', 1)
                    resList = filter(lambda res: res[1].resClass == resClass and res[1].resType == resType, 
                                     resList.items())
                else:
                    resList = filter(lambda res: res[1].resClass == resClass, resList.items())
            else:
                resList = filter(lambda res: res[0] == resName, resList.items())
                
            resList = dict(resList)
        
        fmtstr = '%-4s %-8s %-12s %-8s %-8s'
        
        resStateStr = {ResourceState.R_ONLINE: 'ONLINE',
                       ResourceState.R_OFFLINE: 'OFFLINE',
                       ResourceState.R_BUSY: 'BUSY',
                       ResourceState.R_DETACHED: 'DETACHED'}
        
        print fmtstr % ('ID', 'STATE', 'NAME', 'CLASS', 'TYPE')
        
        for resName, res in resList.iteritems():
            children = map(lambda child: child.childName,
                           filter(lambda child: child.parentName == resName, 
                                  resInfo.childLinks))
            
            print fmtstr % (res.resId,
                            resStateStr[res.state],
                            resName,
                            res.resClass,
                            res.resType)
            
            print '\tChildren: ' + ' '.join(children)
            
            for dataKey, dataValue in res.data.items():
                print '\t%s: %s' % (dataKey, dataValue)
            
            
        
    resources.args = ["[name|.class[.type]]"]

class LoadAgentSelectContext(CLIContext):
    async = True
    
    def doResponse(self, args):
        agentUuid, agentsList, clientList = args
        
        if agentUuid not in agentsList:
            print >> sys.stderr, 'ERROR: No such agent' 
            return self.parent, None
        
        loadAgentId = agentsList[agentUuid].agentId
        agentHostName = agentsList[agentUuid].hostname
        
        isOnline = False
        for client in clientList:
            if client.type == 'load' and client.uuid == agentUuid:
                isOnline = True
                break
        
        agentContext = self.parent.nextContext(LoadAgentContext)
        agentContext.name = '"%s"' % agentHostName
        agentContext.setupAgent(loadAgentId, isOnline)
                
        return agentContext, None

class LoadAgentRootContext(CLIContext):
    name = 'load'
    
    operations = ['list', 'select']
    
    @NextContext(LoadAgentListContext)
    def list(self, args):
        '''Lists all loader agents registered on server'''
        return self.cli.expsvcAgent.listAgents()
    
    @NextContext(LoadAgentSelectContext)
    @inlineCallbacks
    def select(self, args):
        '''Selects single agent to do things'''
        agentUuid = args.pop(0)
        
        agentList = yield self.cli.expsvcAgent.listAgents()
        clientList = yield self.cli.rootAgent.listClients()
        
        returnValue((agentUuid, agentList, clientList))
    select.args = ['<agentUuid>']
    
class AgentListContext(CLIContext):
    name = 'list'
    async = True
    
    @ReturnContext()
    def doResponse(self, agentsList):
        states = {0: 'NEW',
                  1: 'CONNECTED',
                  2: 'ESTABLISHED',
                  3: 'DISCONNECTED'}
        
        authTypes = {0: 'NONE',
                     1: 'MASTER',
                     2: 'ADMIN',
                     3: 'OPERATOR',
                     4: 'USER'}
        
        print '%-4s %-38s %-12s %12s %8s %s' % ('ID', 'UUID', 'CLASS', 
                                                'STATE', 'AUTH', 'ENDPOINT')
        
        for agent in agentsList:
            print '%-4s %-38s %-12s %12s %8s %s' % (agent.id, 
                                                agent.uuid,
                                                agent.type,
                                                states.get(agent.state, 'UNKNOWN'),
                                                authTypes.get(agent.authType, '????'),
                                                agent.endpoint)

class AgentRootContext(CLIContext):
    name = 'agent'
    
    operations = ['list', 'load']
    
    @NextContext(AgentListContext)
    def list(self, args):
        '''Lists all clients registered on server'''
        return self.cli.rootAgent.listClients()
    
    @NextContext(LoadAgentRootContext)
    def load(self, args):
        pass