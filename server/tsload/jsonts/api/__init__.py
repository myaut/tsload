
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



from twisted.internet.defer import Deferred, inlineCallbacks, returnValue

from tsload.jsonts.object import TSObject
from tsload.jsonts import JSONTS

from functools import partial

class TSMethod:
    def __init__(self, returnObj = TSObject.Null(), **kw):
        ''' Method definition - defines method in agent interface in 
        tsload.jsonts.api.* modules.
        
        @param returnObj    return type serdes from jsonts.object. Default is Null
        @param kw           keyword arguments. Names of arguments match with keys in
        protocol.
         '''
        self.returnObj = returnObj
        self.args = kw
        
        self.deserializeResult = partial(self.serdesResult, 
                                         returnObj.deserialize)
        self.serializeResult = partial(self.serdesResult, 
                                       returnObj.serialize)
        
        self.serializeArgs = partial(self.serdesArgs, 'serialize')
        self.deserializeArgs = partial(self.serdesArgs, 'deserialize')
        
    def serdesResult(self, serdesFunc, result):
        if isinstance(result, Deferred):
            def serdesResult(result):
                return serdesFunc(result)
            
            result.addCallback(serdesResult)
            return result 
        
        return serdesFunc(result)
    
    def serdesArgs(self, serdesFuncName, kw):
        for argName, argObj in self.args.iteritems():
            argValue = kw[argName]
            serdesFunc = getattr(argObj, serdesFuncName)
            
            kw[argName] = serdesFunc(argValue)
            
        return kw
        

class TSAgentInterface:     
    class MethodProxy:
        def __init__(self, method, interface):
            self.method = method
            self.interface = interface
            self.agent = interface.agent
        
        def setName(self, name):
            self.name = name
        
        def _callbackResponse(self, response):
            return response
        
        def _callbackError(self, failure):
            code, error = failure.value
            
            raise JSONTS.Error(code, error)
        
        @inlineCallbacks
        def __call__(self, **kwargs):
            d = Deferred()
            d.addCallback(self._callbackResponse)
            d.addErrback(self._callbackError)
            
            kwargs = self.method.serializeArgs(kwargs)
            
            self.agent.sendCommand(d, self.interface.client, 
                                   self.interface.agentId,
                                   self.name, kwargs)
            
            # Asyncronously wait until response or error arrive
            # Then TSAgent will call callback or errback in our Deferred 
            # and yield activates
            result = yield d
            
            returnValue(self.method.deserializeResult(result))
    
    def setClient(self, client):
        self.client = client
    
    def __init__(self, agent, agentId):
        '''Creates new TSAgentInterface proxy; all method descriptors
        are substituted with MethodImpl implementations that simply send
        message to remote agent.
        
        Usage:
        
        class MyAgentInterface(TSAgentInterface):
            myMethod = TSMethod(TSObject.Object(MyReturn),
                                myArg = TSObject.Int())
                                
        In TSAgentCode (requires inlineCallbacks):
        
        agent = createRemoteAgent(10, MyAgentInterface)
        myObj = yield agent.myMethod(myArg = 20)'''
        self.agentId = agentId
        self.agent = agent
        
        self.methods = {}
        
        for methodName in dir(self.__class__):
            method = getattr(self, methodName)
            
            if isinstance(method, TSMethod):
                impl = TSAgentInterface.MethodProxy(method, self)
                impl.setName(methodName)
                
                setattr(self, methodName, impl)
                
class TSMethodImpl:
    def __init__(self, method):
        self.method = method
        
    def __call__(self, f):        
        def wrapper(obj, context, kw):
            kw = self.method.deserializeArgs(kw)
            
            result = f(obj, context, **kw)
        
            return self.method.serializeResult(result)
        
        return wrapper
        
