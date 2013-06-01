import json
import SocketServer
from threading import Event
from select import select

import logging
from logging import StreamHandler, Formatter

class MessageHandler:
    def __init__(self, msgId):
        self.msgId = msgId
        self.event = Event()
        
        self.response = None

class TSException(Exception):
    pass

class TSDisconnect(TSException):
    pass

class TSRequestHandler(SocketServer.ThreadingMixIn, SocketServer.BaseRequestHandler):
    traceDecode = False
    traceMessages = False
    traceClient = False
    
    AE_INTERNAL_ERROR = 202
    
    logger = logging.getLogger('Request')
    logger.setLevel(logging.DEBUG)
    
    def __init__(self, request, client_address, server):
        self.messageId = 0
        self.msgHandlers = {}
        
        self.finished = False
        
        self.logger.info('New connection from %s' % str(client_address))
        
        SocketServer.BaseRequestHandler.__init__(self, request, client_address, server)
    
    def sendMessage(self, msg):
        rawMsg = json.dumps(msg) + '\0'
        
        if self.traceMessages:
            self.logger.debug('Sending %s' % rawMsg)
            
        self.request.send(rawMsg)
    
    def processMessage(self, msg):
        if 'cmd' in msg and 'msg' in msg:
            response = self.server.processCommand(self, msg['cmd'], msg['msg'])
            
            if response is None:
                response = {}
            
            responseMsg = {'id': msg['id'],
                           'response': response}
            
            self.sendMessage(responseMsg)
        elif 'response' in msg or 'error' in msg:
            msgId = msg['id']
            handler = self.msgHandlers[msgId]
            
            handler.response = msg
            handler.event.set()

    def processRawMessage(self, rawMsg):
        response = None
        msg = json.loads(rawMsg)
        
        if self.traceMessages:
            self.logger.debug('Received: %s' % msg)
        
        try:
            response = self.processMessage(msg)
        except Exception as e:
            response = {"id": msg['id'],
                        "error": str(e),
                        "code": TSRequestHandler.AE_INTERNAL_ERROR}
        
        if response is not None:
            self.sendMessage(response)
                
    def getMessageId(self):
        self.messageId += 1
        return self.messageId

    def poll(self, timeout = 0):     
        rlist, wlist, xlist = select([self.request.fileno()], [], 
                                     [self.request.fileno()], timeout)
        
        return len(rlist) != 0 and rlist[0] == self.request.fileno()
    
    def hello(self):
        hello = self.request.recv(1024)[:-1]
        
        if self.traceClient:
            self.logger.debug('Received hello message: ', repr(hello))
        
        helloMsg = json.loads(hello)
        helloResponse = {'id': helloMsg['id'], 'response': { 'agentId': id(self) } }
        
        self.logger.info('Hello from %s!' % helloMsg['msg']['info']['hostName']) 
        
        self.sendMessage(helloResponse)
    
    def handle(self):
        self.hello()
        
        self.server.processClient(self)
        
        while not self.finished:
            if not self.poll(0.5):
                continue
            
            if self.traceClient:
                self.logger.debug('Receiving from %s...' % str(self.client_address))
            
            rawMsg = ''
            while True:
                rawMsg += self.request.recv(4096)
                
                if len(rawMsg) == 0:
                    # Disconnect
                    return
                
                if not self.poll(0.05):
                    break
            
            msgStart, msgEnd = 0, 0
            while msgEnd < len(rawMsg):                
                msgEnd = rawMsg.index('\0', msgStart)
                
                if self.traceDecode:
                    self.logger.debug('Decoding [%d:%d] of %d' % (msgStart, msgEnd, len(rawMsg)))
                
                self.processRawMessage(rawMsg[msgStart:msgEnd])
                
                msgEnd += 1
                msgStart = msgEnd
                
    def finish(self):
        for handler in self.msgHandlers.values():
            handler.event.set()
        
        if self.traceClient:
            self.logger.debug('Client %s finished' % str(self.client_address))
       
    def invoke(self, cmd, **msg):
        msgId = self.getMessageId()
        cmdMessage = {"id": msgId, "cmd": cmd, "msg": msg}
        
        handler = MessageHandler(msgId)
        self.msgHandlers[msgId] = handler
        
        if self.traceClient:
            self.logger.debug('Invoking %s(%s) on %s' % (cmd, msg, str(self.client_address)))
        
        self.sendMessage(cmdMessage)
        
        handler.event.wait()
        
        del self.msgHandlers[msgId]
        
        if handler.response is None:
            raise TSException('Disconnect')
        
        if 'error' in handler.response:
            raise TSException(handler.response['error'])
            
        return handler.response['response']

class TSServer(SocketServer.TCPServer):
    allow_reuse_address = True