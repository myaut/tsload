
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



from tsload import logging

from tsload.jsonts import Flow
from tsload.jsonts.server import TSLocalAgent, TSServerClient

from tsload.jsonts.api import TSMethodImpl
from tsload.jsonts.api.user import *

from tsload.user.model import User, Role, UserAuthError
from tsload.user.localauth import LocalAuth

from storm.locals import create_database
from storm.store import Store
from storm.exceptions import NotOneError

from twisted.internet.defer import inlineCallbacks, returnValue

userAgentUUID = '{2701b3b1-cd8f-457e-9bdd-2323153f16e5}'
userAgentType = 'user'

userAgentId = 1

class TSUserAgent(TSLocalAgent):
    agentId = userAgentId
    
    uuid = userAgentUUID
    agentType = userAgentType
    
    def __init__(self, server, connString):
        TSLocalAgent.__init__(self, server)
        
        self.client.getId()
        
        self.logger = logging.getLogger('UserAgent')
        
        self.rootAgent = server.localAgents[0]
        
        self.agentUsers = {}
        
        self.authServices = {'local': LocalAuth()}
        
        self.database = create_database(connString)
        self.dbStore = Store(self.database)
        
        self.server.listenerFlows.append(Flow(dstAgentId = userAgentId, 
                                              command = 'authUser'))
    
    @TSMethodImpl(UserAgent.authUser)
    def authUser(self, context, **kw):
        @inlineCallbacks
        def implementation(context, userName, userPassword):
            userSet = yield self.dbStore.find(User, User.name == str(userName))
            user = yield userSet.one()
            
            self.logger.info('Authorizing user %s', userName)
            
            if user is None:
                self.logger.warning('Error authorizing user: no such user: %s', userName)
                raise UserAuthError('No such user: %s' % userName)
            
            authMethod = self.authServices[user.authService]
            
            if authMethod.authentificate(user, userPassword):
                agentId = context.client.getId()
                self.agentUsers[agentId] = user.id
                
                roles = yield user.roles
                role = self._setupRoles(context.client, roles)
            
            userDescr = TSUserDescriptor()
            userDescr.name = user.gecosName
            userDescr.role = role
            
            returnValue(userDescr)
        
        return implementation(context, **kw)
    
    def _setupRoles(self, client, roles):
        # First pass - identify maximum role
        maxRole = TSServerClient.AUTH_NONE
        
        for role in roles:
            if role.role == 'admin':
                maxRole = TSServerClient.AUTH_ADMIN
            elif role.role == 'operator' and maxRole != TSServerClient.AUTH_ADMIN:
                maxRole = TSServerClient.AUTH_OPERATOR
            else:
                maxRole = TSServerClient.AUTH_USER
            
        
        client.authorize(maxRole)
        
        if maxRole != TSServerClient.AUTH_ADMIN:
            # TODO: For user/operator need to set ACLs
            pass
        
        return maxRole
    
    def onDisconnect(self):
        self.dbStore.close()
