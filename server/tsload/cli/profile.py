'''
Created on Sep 19, 2013

@author: myaut
'''

from tsload.jsonts import JSONTS, tstimeToUnixTime
from tsload.cli.context import CLIContext, NextContext, ReturnContext, SameContext
from tsload.jsonts.api.expsvc import *

from twisted.internet.defer import inlineCallbacks, returnValue

class ProfileConfigurationContext(CLIContext):
    operations = ['info', 'commit']
    
    @SameContext()
    def info(self, args):
        print self.profile
        
        return args
    
    @SameContext()
    @inlineCallbacks
    def commit(self, args):
        yield self.cli.expsvcAgent.configureProfile(profileName = self.profileName,
                                                    profile = self.profile)
        
        print 'Experiment profile saved'

class ProfileRootContext(CLIContext):
    name = 'profile'
    operations = ['list', 'create', 'select']
    
    @SameContext()
    @inlineCallbacks
    def list(self, args):
        profiles = yield self.cli.expsvcAgent.listProfiles()
        
        for profile in profiles:
            print profile
    
    def create(self, args):
        profileName = args.pop()
        profile = TSExperimentProfile.createEmptyProfile()
        
        config = ProfileConfigurationContext(self, self.cli)
        config.profile = profile
        config.profileName = profileName
        
        config.name = repr(profileName)
        
        return config, args
    
    @SameContext()
    def select(self, args):
        pass