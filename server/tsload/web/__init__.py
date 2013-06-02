from tsload.jsonts import JSONTS
from tsload.jsonts.agent import TSAgent

from tsload.jsonts.api.user import UserAgent

from twisted.internet.defer import inlineCallbacks

HOST = 'localhost'
PORT = 9090

class TSWebAgent(TSAgent):
    '''Web-interface agent
    
    Supports only user-based authentification. Calls eventListener deferred if error
    were encountered or everything went fine.'''
    STATE_NEW             = 0
    STATE_CONNECTED       = 1
    STATE_CONN_ERROR      = 2
    STATE_AUTHENTIFICATED = 3
    STATE_AUTH_ERROR      = 4
    
    state = STATE_NEW
    
    def setEventListener(self, el):
        self.eventListener = el
    
    def setAuthData(self, userName, userPassword):
        self.userName = userName
        self.userPassword = userPassword
    
    @inlineCallbacks
    def gotAgent(self):
        self.state = TSWebAgent.STATE_CONNECTED
        self.userAgent = self.createRemoteAgent(1, UserAgent)
        
        try:
            user = yield self.userAgent.authUser(userName = self.userName, 
                                                 userPassword = self.userPassword)
            
            self.state = TSWebAgent.STATE_AUTHENTIFICATED
            self.gecosUserName = user.name
            
            self.eventListener.callback(self)
        except JSONTS.Error as je:
            self.state = TSWebAgent.STATE_AUTH_ERROR
            self.authError = str(je)
            
            self.eventListener.callback(self)
    
    def gotConnectionError(self, error):
        self.state = TSWebAgent.STATE_CONN_ERROR
        self.connectionError = error.getErrorMessage()
        
        self.eventListener.callback(self)
        
    