'''
Created on 09.05.2013

@author: myaut
'''

import sys
import shlex
import socket
import traceback

from tsload.jsonts import JSONTS
from tsload.jsonts.agent import TSAgent

from tsload.cli.context import CLIContext, NextContext, SameContext
from tsload.cli.agent import AgentRootContext

from tsload.jsonts.api.user import UserAgent

from twisted.internet import reactor
from twisted.internet.defer import Deferred, inlineCallbacks

class RootContext(CLIContext):
    name = ''
    operations = ['agent']
    
    @NextContext(AgentRootContext)
    def agent(self, args):
        '''Agent administration'''
        return args
    
    @SameContext()
    def doResponse(self, response):
        pass

class TSAdminCLIAgent(TSAgent):
    def __init__(self):
        self.context = RootContext(None, self)
    
    def setAuthType(self, authUser, authMasterKey, authPassword=None):
        self.authUser = authUser
        self.authPassword = authPassword
        self.authMasterKey = authMasterKey
    
    @inlineCallbacks
    def gotAgent(self):
        self._resetSigInt()
        
        # Try to authentificate CLI agent    
        try:
            if self.authMasterKey != None:
                _ = yield self.rootAgent.authMasterKey(masterKey=self.authMasterKey)
                self.ask()
            elif self.authUser != None:
                import getpass
                self.userAgent = self.createRemoteAgent(1, UserAgent)
                
                password = self.authPassword
                if password is None:
                    password = getpass.getpass()
                
                user = yield self.userAgent.authUser(userName=self.authUser, 
                                                     userPassword=password)
                self.ask()
        except JSONTS.Error as je:
            print >> sys.stderr, 'ERROR %d: %s' % (je.code, je.error)
            reactor.stop()
    
    def _resetSigInt(self):
        # Twisted reactor installs it's own SIGINT handler, which breaks _readArgs logic
        # It can be overriden by installSignalHandlers=False, but we loose SIGTERM etc.
        # Manually reset to SIGINT which cause sys.stdin.readline() throw KeyboardInterrupt()
        # NOTE: should be called after reactor is initialized, from it's context
        import signal
        signal.signal(signal.SIGINT, signal.default_int_handler)
    
    def _readArgs(self):
        prompt = 'TS %s> ' % self.context.path()
        command = ''
        done = False
        
        interrupts = 0
        
        sys.stdout.flush()
        
        while not done:
            try:
                sys.stdout.write(prompt)
                sys.stdout.flush()
                input = sys.stdin.readline()
                
                command += input.strip()
            
                args = shlex.split(command)
                done = True
            except ValueError:
                prompt = '> '
            except KeyboardInterrupt:
                interrupts += 1
                if interrupts == 2:
                    raise KeyboardInterrupt()
                
                print 'Interrupted'
                
                command = ''
                prompt = 'TS %s> ' % self.context.path()
                
        return args
    
    def ask(self):
        args = []
        
        while not args:
            try:
                args = self._readArgs()
            except KeyboardInterrupt:
                reactor.stop()
                return
        
        self._execute(args)
    
    def _executeImpl(self, args):
        origContext = self.context
        
        def gotResponse(response):
            self.context, _ = self.context.doResponse(response)
            reactor.callLater(0, self.ask)
            
        def gotError(failure):
            self.context = origContext 
            
            print >> sys.stderr, 'ERROR %s' % failure
            reactor.callLater(0, self.ask)
        
        while not self.context.async and args:
            self.context, d = self.context.call(args)
        
        if isinstance(d, Deferred):
            d.addCallback(gotResponse)
            d.addErrback(gotError)
        else:
            reactor.callLater(0, self.ask)
    
    def _execute(self, args):
        try:                
            self._executeImpl(args)
        except SystemExit:
            reactor.stop()
            return
        except Exception as e:
            traceback.print_exc(file = sys.stderr)
            
            reactor.callLater(0, self.ask)
    
    def gotError(self, code, error):
        print >> sys.stderr, 'ERROR %d: %s' % (code, error)
        reactor.stop()
    
    def help(self, args):
        pass
    