import sys
import json
import time

import copy
import traceback

from tsload import logging

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
    
    instance = None
    
    class Error(Exception):
        def __init__(self, code, error):
            self.code = code
            self.error = error
            
            if JSONTS.instance is not None:
                JSONTS.instance.logger.error('ERROR %d: %s', code, error)
            
        def __str__(self):            
            return 'ERROR %d: %s' % (self.code, self.error)
    
    def __init__(self, factory):
        # For logging purposes use first JSONTS object
        if JSONTS.instance is None:
            JSONTS.instance = self
        
        self.factory = factory
        self.state = JSONTS.STATE_NEW
        self.buffer = ''
        
        self.logger = logging.getLogger('JSONTS')
        
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
            raise JSONTS.Error(JSONTS.AE_CONNECTION_ERROR, 
                               'Connection state is %d' % self.state)
        
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
    
    def __repr__(self):
        return '<TSLocalClientProxy #%d>' % (self.client.getId())
    __str__ = __repr__
    
def datetimeToTSTime(dt):
    """JSON-TS uses INT64 with nanosecond resolution from Unix Epoch to transfer timestamps
    while Storm ORM uses Python module datetime. 
    
    This function converts it from datetime.datetime to JSON-TS time."""
    t = int(time.mktime(dt.timetuple()) * 1000000000)
    return t + (dt.time().microsecond * 1000)

def tstimeToUnixTime(tst):
    return tst / 1000000000.