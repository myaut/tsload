'''
Created on 01.06.2013

@author: myaut
'''

from tsload.jsonts.object import TSObject as tso

from tsload.jsonts.api import TSAgentInterface, TSMethod

class TSUserDescriptor(tso):
    name = tso.String()
    
    # See tsload.jsonts.server.TSServerClient - AUTH_* contstants
    AUTH_ADMIN = 2
    AUTH_OPERATOR = 3
    AUTH_USER = 4
    
    role = tso.Int()

class UserAgent(TSAgentInterface):
    authUser = TSMethod(tso.Object(TSUserDescriptor),
                        userName = tso.String(),
                        userPassword = tso.String())