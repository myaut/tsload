'''
Created on 01.06.2013

@author: myaut
'''

from tsload.jsonts.object import TSObject as tso

from tsload.jsonts.api import TSAgentInterface, TSMethod

class TSUserDescriptor(tso):
    name = tso.String()

class UserAgent(TSAgentInterface):
    authUser = TSMethod(tso.Object(TSUserDescriptor),
                        userName = tso.String(),
                        userPassword = tso.String())