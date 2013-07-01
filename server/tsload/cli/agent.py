'''
Created on 06.05.2013

@author: myaut
'''

import sys
import time

from tsload.jsonts import JSONTS, tstimeToUnixTime
from tsload.cli.context import CLIContext, NextContext, ReturnContext, SameContext

from twisted.internet.defer import inlineCallbacks, returnValue, waitForDeferred

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

class LoadAgentRootContext(CLIContext):
    name = 'load'
    
    operations = ['list', 'select']
    
    @NextContext(LoadAgentListContext)
    def list(self, args):
        '''Lists all loader agents registered on server'''
        return self.cli.expsvcAgent.listAgents()
    
    def select(self, args):
        '''Selects single agent to do things'''
        # agentId = int(args.pop(0))
        
        # self.cli.listAgents()
        
        # selectContext = self.nextContext(AgentSelectContext)
        # selectContext.setAgentId(agentId)
        # return selectContext, []
    select.args = ['<agentId>']
    

class AgentSelectContext(CLIContext):
    async = True
    
    def setAgentId(self, agentId):
        self.agentId = agentId
    
    def doResponse(self, agentsList):
        if str(self.agentId) in agentsList:
            agentContext = self.parent.nextContext(AgentContext)
            agentContext.setAgentId(self.agentId)
            return agentContext, []
        
        print >> sys.stderr, 'No such agent: %d', self.agentId
        return self.parent, []

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
    
    operations = ['list', 'select', 'load']
    
    @NextContext(AgentListContext)
    def list(self, args):
        '''Lists all clients registered on server'''
        return self.cli.rootAgent.listClients()
    
    def select(self, args):
        '''Selects single agent to do things'''
        agentId = int(args.pop(0))
        
        self.cli.listAgents()
        
        selectContext = self.nextContext(AgentSelectContext)
        selectContext.setAgentId(agentId)
        return selectContext, []
    select.args = ['<agentId>']
    
    @NextContext(LoadAgentRootContext)
    def load(self, args):
        return []