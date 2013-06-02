import sys
import json

import copy
import traceback

from twisted.internet.protocol import Protocol, Factory

class AccessRule:
    def __init__(self, srcAgentId = None, dstAgentId = None, 
                 command = None):
        self.srcAgentId = srcAgentId
        self.dstAgentId = dstAgentId
        self.command = command
        
    def __eq__(self, flow):
        if self.dstAgentId is not None and flow.dstAgentId != self.dstAgentId:
            return False
        
        if self.srcAgentId is not None and flow.srcAgentId != self.srcAgentId:
            return False
        
        if self.command is not None and flow.command != self.command:
            return False
        
        return True
    
    def __str__(self):
        return '<%s %s:%s:%s>' % (self.__class__.__name__, 
                                  self.srcAgentId, self.dstAgentId, self.command)

class Flow(AccessRule):
    def clone(self, srcMsgId, dstMsgId):
        flow = copy.copy(self)
        flow.setMsgIds(srcMsgId, dstMsgId)
        
        return flow
    
    def setMsgIds(self, srcMsgId, dstMsgId):
        self.srcMsgId = srcMsgId
        self.dstMsgId = dstMsgId 
        
    def isPermanent(self):
        return self.msgId is None

class JSONTS(Protocol):
    STATE_NEW          = 0
    STATE_CONNECTED    = 1
    STATE_ESTABLISHED  = 2
    STATE_DISCONNECTED = 3
    
    # Protocol errors
    AE_COMMAND_NOT_FOUND    = 100
    AE_MESSAGE_FORMAT       = 101
    AE_INVALID_DATA         = 102
    AE_INVALID_STATE        = 103
    
    AE_INVALID_AGENT        = 200
    AE_ACCESS_DENIED        = 201
    AE_CONNECTION_ERROR     = 202
    
    AE_INTERNAL_ERROR       = 300
    
    class Error(Exception):
        def __init__(self, code, error):
            self.code = code
            self.error = error
            
        def __str__(self):
            return 'ERROR %d: %s' % (self.code, self.error)
    
    def __init__(self, factory):
        self.factory = factory
        self.state = JSONTS.STATE_NEW
        self.buffer = ''
        
        self.commandIdGenerator = iter(xrange(sys.maxint))
    
    def __str__(self):
        return '<%s %d>' % (self.__class__.__name__, self.state)
    
    def connectionMade(self):
        self.state = JSONTS.STATE_CONNECTED
    
    def connectionLost(self, reason):
        self.state = JSONTS.STATE_DISCONNECTED
    
    def dataReceived(self, data):
        data = self.buffer + data
        while data:
            chunk, sep, data = data.partition('\0')
            
            if not sep:
                # Received data was incomplete
                self.buffer = chunk
                break
            
            self.factory.doTrace('[%d] received: %s', self.agentId, repr(chunk))
            
            self.processRawMessage(chunk)
    
    def sendMessage(self, msg):
        if self.state not in [JSONTS.STATE_CONNECTED, JSONTS.STATE_ESTABLISHED]:
            return
        
        rawMsg = json.dumps(msg) + '\0'
        
        self.factory.doTrace('[%d] sending: %s', self.agentId, repr(rawMsg))
        
        self.transport.write(rawMsg)
        
    def sendCommand(self, agentId, command, msgBody):
        commandId = next(self.commandIdGenerator)
        self.sendMessage({'agentId': agentId, 
                          'id': commandId, 
                          'cmd': command, 
                          'msg': msgBody})
        
        return commandId
        
    def sendResponse(self, agentId, msgId, response):
        self.sendMessage({'agentId': agentId, 
                          'id': msgId, 
                          'response': response})
        
    def sendError(self, agentId, msgId, error, code):
        self.sendMessage({'agentId': agentId, 
                          'id': msgId, 
                          'error': error, 
                          'code': code})
    
    def processRawMessage(self, rawMsg):
        msg = json.loads(rawMsg)
        self.processMessage(msg)
    
    def processMessage(self, msg):
        origMsgId = msg.get('id', -1)
        try:
            self.factory.processMessage(self, msg)
        except JSONTS.Error as je:
            self.sendError(self.agentId, origMsgId, je.error, je.code)
        except Exception as e:
            print >> sys.stderr, '= INTERNAL ERROR ='
            traceback.print_exc(file = sys.stderr)
            print >> sys.stderr, '=================='
            
            self.sendError(self.agentId, origMsgId, str(e), JSONTS.AE_INTERNAL_ERROR)
            