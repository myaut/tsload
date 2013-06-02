'''
Created on 07.05.2013

@author: myaut
'''

import sys
import traceback

from tsload.jsonts import AccessRule, Flow, JSONTS
from tsload.jsonts.agent import TSAgent, CallContext

from twisted.internet.protocol import Factory
from twisted.internet import reactor, task

import uuid

# First agent id used by remote agents
remoteAgentId = 8

class TSServerClient(JSONTS):
    uuid = None
    
    AUTH_NONE = 0
    AUTH_MASTER = 1
    AUTH_ADMIN = 2
    AUTH_OPERATOR = 3
    AUTH_USER = 4
    
    agentUuid = None
    agentType = 'unknown'
    
    def __init__(self, factory, agentId):
        self.agentId = agentId
        
        self.flows = {}
        self.acl = []
        
        self.auth = TSServerClient.AUTH_NONE
        
        self.endpointStr = 'local'
        
        JSONTS.__init__(self, factory)
        
    def addAccessRule(self, ar):
        self.acl.append(ar)
    
    def getId(self):
        return self.agentId
    
    def setId(self, agentId):
        self.agentId = agentId
    
    def authorize(self, authType):
        self.authType = authType
        
    def setEndpointStr(self, endpointStr):
        self.endpointStr = endpointStr
    
    def setAgentInfo(self, agentType, agentUuid):
        self.agentType = agentType
        self.agentUuid = agentUuid
    
    def checkACL(self, flow):
        '''Validates flow according to client ACLs. If client
        was authentificated with server master key, validation will always
        be successful.
        
        @return True if validation successful
        
        FIXME: Implement indexes for ACLs'''
        if self.auth == TSServerClient.AUTH_MASTER or \
           self.auth == TSServerClient.AUTH_ADMIN:
                return True
        
        for ar in self.acl:
            if ar == flow:
                return True
        
        return False
    
    def addFlow(self, flow):
        self.factory.doTrace('Added flow %s [%d -> %d] to client %d', flow, flow.srcMsgId, flow.dstMsgId, self.agentId)
        
        self.flows[flow.dstAgentId, flow.dstMsgId] = flow
    
    def findFlow(self, agentId, msgId):
        '''Finds flow with corresponding agentId/msgId, and
        deletes it from flows table. 
        
        @param agentId: destination agent id
        @param msgId: destination message id
        
        NOTE: During if command forwards A -> B, response
        forwards from B to A, so you should use source agent id's for responses'''
        try:
            flow = self.flows[agentId, msgId]
            del self.flows[agentId, msgId]
            return flow
        except KeyError:
            return None

class TSLocalClientProxy(JSONTS):
    '''Class helper that proxies responses made by local clients to server's
    main processing engine to reset agentId/msgId according to flow. 
    Other operations are proxied directly to client with no changes'''
    def __init__(self, server, client, localClient):
        self.server = server
        self.client = client
        self.localClient = localClient
    
    def sendMessage(self, msg):
        self.server.processMessage(self.localClient, msg)
        
    def __getattr__(self, name):
        return getattr(self.client, name)

class TSServer(Factory):
    '''TSServer is a class that handles JSON-TS servers and local agents.
    
    JSON-TS is peer-to-peer protocol, so any agent can communicate with any other agent,
    but it would be wasteful in terms of TCP connections (and hard to implement behind
    NAT's or firewalls), so agents are using Server as intermediate transport.
    
    Command messages are delivered directly to agent, according to agentId field, 
    if corresponding access rule exist or through listener flows mechanism. Server create 
    special descriptor, called flow (see class TSFlow), that maps source/target agent ids
    and message ids.
    Error/Responses are routed back according to flows saved in client descriptor.
    
    Server cannot accept commands, so 'local' agents implemented to control
    server behavior: RootAgent and UserAgent. '''
    def __init__(self):        
        self.clients = {}
        self.localAgents = {}
        
        self.listenerFlows = []
        
        self.msgCounter = iter(xrange(1, sys.maxint))
        self.agentIdGenerator = iter(xrange(remoteAgentId, sys.maxint))
        
        self.deadCleaner = task.LoopingCall(self.cleanDeadAgents)
        self.deadCleaner.start(60, False)
        
        self.generateMasterKey()
        
    def generateMasterKey(self):
        self.masterKey = uuid.uuid1()
        self.doTrace('Master key is %s', self.masterKey)
        
        # FIXME: correct directory
        masterFile = file('master.key', 'w')
        masterFile.write(str(self.masterKey))
        masterFile.close()
    
    def doTrace(self, fmt, *args):
        print fmt % args 
    
    @classmethod
    def createServer(klass, port):
        server = klass()
        
        reactor.listenTCP(port, server)
        
        return server
    
    def createLocalAgent(self, agentKlass, agentId, *args, **kw):
        agent = agentKlass(self, *args, **kw)
        
        agent.client.authorize(TSServerClient.AUTH_MASTER)
        
        self.localAgents[agentId] = agent
        self.clients[agentId] = agent.client
    
    def buildProtocol(self, addr):
        agentId = next(self.agentIdGenerator)
        client = TSServerClient(self, agentId)
        client.setEndpointStr(addr.host + ':' + str(addr.port))
        
        self.clients[agentId] = client
        
        return client
    
    def cleanDeadAgents(self):
        '''Cleans disconnected agent. Called in a loop task, so
        no need to call it directly'''
        deadAgentIds = []
        
        # FIXME: should process routes
        
        for agentId, client in self.clients.items():
            if client.state == JSONTS.STATE_DISCONNECTED:
                deadAgentIds.append(agentId)
                
        for agentId in deadAgentIds:
            self.doTrace('Cleaned agent %d', agentId)
            del self.clients[agentId]
    
    def processMessage(self, srcClient, message):
        '''Main method of server that routes messages between agents'''
        srcAgentId = srcClient.getId()
        srcMsgId = message['id']
        
        dstAgentId = message['agentId']
        dstMsgId = next(self.msgCounter)
        
        self.doTrace('Process message %s from %d', message, srcAgentId)
        
        try:
            dstClient = self.clients[dstAgentId]
        except KeyError:
            if 'cmd' in message:
                raise JSONTS.Error(JSONTS.AE_INVALID_AGENT, 
                                   'Invalid agent #%d' % dstAgentId)
            return
        
        self.doTrace('Process message %s(%d, %d) -> %s(%d, %d)',
                     srcClient, srcAgentId, srcMsgId, 
                     dstClient, dstAgentId, dstMsgId)
        
        if 'cmd' in message:
            command = message['cmd']
            
            flow = Flow(srcAgentId = srcAgentId,
                        dstAgentId = dstAgentId, 
                        command = command)
            
            # The only way to communicate for unauthorized 
            if srcClient.auth == TSServerClient.AUTH_NONE:
                for listener in self.listenerFlows:
                    self.doTrace('Checking flow %s against listener rule %s', flow, listener)
                    
                    if listener == flow:
                        dstMsgId = next(self.msgCounter)
                        flowClone = flow.clone(srcMsgId, dstMsgId)
                        dstClient.addFlow(flowClone)
                        
                        message['id'] = dstMsgId
                        message['agentId'] = listener.dstAgentId
                        self.deliverMessage(srcClient, message)
            else:
                flow.setMsgIds(srcMsgId, dstMsgId)
                
                if not srcClient.checkACL(flow):
                    raise JSONTS.Error(JSONTS.AE_ACCESS_DENIED, 'Access is denied')
                
                dstClient.addFlow(flow)
                
                message['id'] = dstMsgId
                self.deliverMessage(srcClient, message)
        else:
            flow = srcClient.findFlow(srcAgentId, srcMsgId)
            
            if flow is None:
                return
            
            self.doTrace('Found flow %s [%d -> %d]', flow, flow.srcMsgId, flow.dstMsgId)
            
            message['id'] = flow.srcMsgId
            message['agentId'] = flow.srcAgentId
            
            self.deliverMessage(srcClient, message)
      
    def deliverMessage(self, srcClient, message):  
        dstAgentId = message['agentId']
        
        self.doTrace('Deliver message -> %d', dstAgentId)
        
        if dstAgentId < remoteAgentId:
            srcClient = TSLocalClientProxy(self, srcClient, self.clients[dstAgentId])
            self.localAgents[dstAgentId].processMessage(srcClient, message)
        else:
            dstClient = self.clients[dstAgentId]
            dstClient.sendMessage(message)
            
            