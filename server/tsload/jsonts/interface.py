'''
Created on 09.05.2013

@author: myaut
'''

class TSAgentInterface:
    class Invokation:
        def __init__(self, msgId):
            self.errback = None
            self.callback = None
            self.msgId = msgId
            
        def getMsgId(self):
            return self.msgId
        
        def setCallbacks(self, callback, errback):
            self.callback = callback
            self.errback = errback
    
    class MethodImpl:
        def __init__(self, method, interface):
            self.method = method
            self.interface = interface
            self.client = interface.client
            
        def __call__(self, **kwargs):
            msgId = self.client.sendCommand(self.interface.agentId, 
                                            self.method.name,
                                            kwargs)
            call = TSAgentInterface.Invokation(msgId)
            
            return call
    
    class Method:
        def __init__(self, name):
            self.name = name
    
    def __init__(self, client, agentId):
        self.agentId = agentId
        self.client = client
        
        self.methods = {}
        
        for mname in dir(self.__class__):
            method = getattr(self, mname)
            
            if isinstance(method, TSAgentInterface.Method):
                impl = TSAgentInterface.MethodImpl(method, self)
                setattr(self, mname, impl)
        
      
class RootAgent(TSAgentInterface):
    hello = TSAgentInterface.Method('hello')
    authMasterKey = TSAgentInterface.Method('authMasterKey')
    listClients = TSAgentInterface.Method('listClients')
    
class UserAgent(TSAgentInterface):
    authUser = TSAgentInterface.Method('authUser')