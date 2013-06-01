'''
Created on 06.05.2013

@author: myaut
'''

import sys

from tsload.cli import CLIContext, NextContext, ReturnContext

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
        
        print '%-4s %-16s %-12s %s' % ('ID', 'UUID', 'CLASS', 'STATE')
        
        for agentIdStr, agent in agentsList.items():
            print '%-4s %-16s %-12s %s' % (agentIdStr, 
                                           agent['uuid'],
                                           agent['class'],
                                           states.get(agent['state'], 'UNKNOWN'))
            
        return []

class AgentRootContext(CLIContext):
    name = 'agent'
    
    operations = ['list', 'select']
    
    @NextContext(AgentListContext)
    def list(self, args):
        '''Lists all agents registered on server'''
        self.cli.doCall(self.cli.rootAgent.listClients())
        
        return []
    
    def select(self, args):
        '''Selects single agent to do things'''
        agentId = int(args.pop(0))
        
        self.cli.call(self.cli.listAgents())
        
        selectContext = self.nextContext(AgentSelectContext)
        selectContext.setAgentId(agentId)
        return selectContext, []
    select.args = ['<agentId>']