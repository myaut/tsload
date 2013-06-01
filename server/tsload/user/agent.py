'''
Created on May 13, 2013

@author: myaut
'''

from tsload.jsonts import Flow
from tsload.jsonts.agent import TSAgent
from tsload.jsonts.server import TSServerClient

from tsload.user import User, Role, UserAuthError
from tsload.user.localauth import LocalAuth

from storm.locals import create_database
from storm.store import Store
from storm.exceptions import NotOneError

from twisted.internet.defer import inlineCallbacks, returnValue, waitForDeferred

userAgentUUID = '{2701b3b1-cd8f-457e-9bdd-2323153f16e5}'
userAgentType = 'user'

userAgentId = 1

class TSUserAgent(TSAgent):
    uuid = userAgentUUID
    agentType = userAgentType
    
    def __init__(self, server, connString):
        self.server = server
        
        self.client = TSServerClient(self, userAgentId)
        self.client.setAgentInfo(self.agentType, self.uuid)
        
        self.client.getId()
        
        self.rootAgent = server.localAgents[0]
        
        self.agentUsers = {}
        
        self.authServices = {'local': LocalAuth()}
        
        self.database = create_database(connString)
        self.dbStore = Store(self.database)
        
        self.server.listenerFlows.append(Flow(dstAgentId = userAgentId, 
                                              command = 'authUser'))
    
    def authUser(self, context, userName, userPassword):
        @inlineCallbacks
        def implementation(context, userName, userPassword):
            userSet = yield self.dbStore.find(User, User.name == str(userName))
            user = yield userSet.one()
            
            if user is None:
                raise UserAuthError('No such user: %s' % userName)
            
            authMethod = self.authServices[user.authService]
            
            if authMethod.authentificate(user, userPassword):
                agentId = context.client.getId()
                self.agentUsers[agentId] = user.id
                
                # need to set up roles
            
            returnValue({'name': user.gecosName})
        
        return implementation(context, userName, userPassword)
    
    def onDisconnect(self):
        self.dbStore.close()