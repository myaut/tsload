
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



import sys

from tsload import logging

from tsload.jsonts import JSONTS, TSLocalClientProxy
from tsload.jsonts.api.root import RootAgent

from twisted.internet.protocol import Factory
from twisted.internet.endpoints import TCP4ClientEndpoint
from twisted.internet.defer import Deferred
from twisted.internet import reactor

from twisted.internet.defer import inlineCallbacks, returnValue

import uuid
import inspect

class CallContext:
    def __init__(self, client, msgId):
        self.client = client
        self.agentId = self.client.getId()
        self.msgId = msgId

class TSAgentClient(JSONTS):
    uuid = None
    
    def __init__(self, factory, agentId):
        self.agentId = agentId
        
        JSONTS.__init__(self, factory)
        
    def getId(self):
        return self.agentId

class TSAgent(Factory):
    '''Generic agent implementation. Used both by local and remote agents, however
    construction phases are differ:
    - Local agents created by directly calling it's constructor 
    - Remote agents created by TSRemoteAgentImpl.createAgent('type', HOST, PORT)'''
    
    uuid = None
    agentType = 'generic'
    
    @classmethod
    def createAgent(klass, agentType, host, port):
        agent = klass()
        
        if agent.uuid is None:
            agent.uuid = str(uuid.uuid1())
        
        agent.agentType = agentType
        agent.agents = {}
        agent.calls = {}
        
        agent.endpoint = TCP4ClientEndpoint(reactor, host, port)
        d = agent.endpoint.connect(agent)
        d.addCallback(agent.gotClient)
        d.addErrback(agent.gotConnectionError)
        
        return agent
    
    def doTrace(self, fmt, *args):
        logging.trace('agent', fmt, *args)
    
    def buildProtocol(self, addr):
        return TSAgentClient(self, -1)
    
    def createRemoteAgent(self, agentId, agentKlass):
        '''Create remote agent interface object
        
        @param agentId remote agent id
        @param agentKlass remote agent interface from tsload.jsonts.api.*'''
        agent = agentKlass(self, agentId)
        self.agents[agentId] = agent
        
        agent.setClient(self.client)
        
        return agent
        
    def processMessage(self, srcClient, message):
        srcAgentId = srcClient.getId()
        srcMsgId = message['id']
        
        dstAgentId = message['agentId']
        
        self.doTrace('Process message %s(%d, %d)', srcClient, srcAgentId, srcMsgId)
        
        if 'cmd' in message:
            command = message['cmd']
            cmdArgs = message['msg']
            
            if dstAgentId != self.client.getId():
                raise JSONTS.Error(JSONTS.AE_INVALID_AGENT,
                                   'Invalid destination agent: ours is %d, received is %d' % (self.client.getId(), 
                                                                                              dstAgentId))
            
            if type(cmdArgs) != dict:
                raise JSONTS.Error(JSONTS.AE_MESSAGE_FORMAT, 
                                   'Message should be dictionary, not a %s' % type(cmdArgs))
            
            try:
                method = getattr(self, command)
            except AttributeError as ae:
                raise JSONTS.Error(JSONTS.AE_COMMAND_NOT_FOUND, str(ae))
            
            context = CallContext(srcClient, srcMsgId)
            
            response = method(context, cmdArgs)
            
            if isinstance(response, Deferred):
                # Add closures callback/errbacks to deferred and return
                def deferredCallback(response):
                    self.doTrace('Got response (deferred) %s', response)
                    srcClient.sendResponse(srcClient.getId(), srcMsgId, response)
                
                def deferredErrback(failure):
                    self.doTrace('Got error (deferred) %s', str(failure))
                    srcClient.sendError(srcClient.getId(), srcMsgId, failure.getErrorMessage(), 
                                        JSONTS.AE_INTERNAL_ERROR)
                
                response.addCallback(deferredCallback)
                response.addErrback(deferredErrback)
                
                return
            
            self.doTrace('Got response %s', response)
            
            srcClient.sendResponse(srcClient.getId(), srcMsgId, response)
        else:
            # Notify MethodProxy deferred that response arrived
            call = self.calls[srcMsgId]
            if 'response' in message:
                response = message['response'] 
                call.callback(response)
            elif 'error' in message:
                error = message['error']
                code = message['code']
                call.errback((code, error))
            del self.calls[srcMsgId]
    
    def sendCommand(self, d, client, *args, **kwargs):
        '''Send command to remote agent. 
        When agent will reply, processResponse or processError will be called,
        so they trigger deferred callback/errback
        
        @param d Deferred object which callbacks/errbacks will be triggered when response/error arrives
        @param client Transport used to send message. See createRemoteAgent() code for details
        @param args passed to client.sendCommand
        @param kwargs passed to client.sendCommand
        
        Helper for MethodImpl.__call__'''
        msgId = client.sendCommand(*args, **kwargs)
        
        self.calls[msgId] = d
    
    @inlineCallbacks
    def gotClient(self, client):
        # Connection was successful
        self.client = client
        self.rootAgent = self.createRemoteAgent(0, RootAgent)
        
        helloMsg = yield self.rootAgent.hello(agentType = self.agentType, 
                                              agentUuid = self.uuid)
        
        self.agentId = helloMsg.agentId
        
        self.doTrace('Got hello response %s', helloMsg)
        
        self.gotAgent()
    
    def gotConnectionError(self, failure):
        '''Generic implementation for connection error event.
        Falls back to gotError. May be overriden'''
        self.gotError(JSONTS.AE_CONNECTION_ERROR, str(failure))
    
    def gotError(self, code, error):
        '''Generic implementation for JSONTS error - simply dumps error
        to stderr. May be overriden'''
        print >> sys.stderr, 'ERROR %d: %s' % (code, error)
    
    def gotAgent(self):
        '''Method called then agent established connection'''
        pass
    
    def disconnect(self):
        self.endpoint.disconnect()    
        
