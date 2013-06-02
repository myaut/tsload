'''
Created on 06.05.2013

@author: myaut
'''

import sys

from tsload.jsonts import JSONTS
from tsload.cli.context import CLIContext, NextContext, ReturnContext, SameContext

from twisted.internet.defer import inlineCallbacks, returnValue, waitForDeferred

class AgentContext(CLIContext):
    def setAgentId(self, agentId):
        self.agentId = agentId
        self.name = str(agentId)

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
        
        print '%-4s %-38s %-12s %12s %s' % ('ID', 'UUID', 'CLASS', 'STATE', 'ENDPOINT')
        
        for agent in agentsList:
            print '%-4s %-38s %-12s %12s %s' % (agent.id, 
                                                agent.uuid,
                                                agent.type,
                                                states.get(agent.state, 'UNKNOWN'),
                                                agent.endpoint)

class AgentRootContext(CLIContext):
    name = 'agent'
    
    operations = ['list', 'select']
    
    @NextContext(AgentListContext)
    def list(self, args):
        '''Lists all agents registered on server'''
        return self.cli.rootAgent.listClients()
    
    def select(self, args):
        '''Selects single agent to do things'''
        agentId = int(args.pop(0))
        
        self.cli.listAgents()
        
        selectContext = self.nextContext(AgentSelectContext)
        selectContext.setAgentId(agentId)
        return selectContext, []
    select.args = ['<agentId>']