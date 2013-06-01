'''
Created on 09.05.2013

@author: myaut
'''

import sys
import shlex
import socket

from tsload.jsonts.agent import TSAgent
from tsload.cli import CLIContext, NextContext
from tsload.cli.agent import AgentRootContext
from tsload.jsonts.interface import UserAgent

from twisted.internet import reactor

class RootContext(CLIContext):
    name = ''
    operations = ['agent']
    
    @NextContext(AgentRootContext)
    def agent(self, args):
        '''Agent administration'''
        return args
    
    def doResponse(self, response):
        return self, []

class TSAdminCLIAgent(TSAgent):
    def __init__(self):
        self.context = RootContext(None, self)
    
    def setAuthType(self, authUser, authMasterKey, authPassword=None):
        self.authUser = authUser
        self.authPassword = authPassword
        self.authMasterKey = authMasterKey
    
    def gotAgent(self):
        if self.authMasterKey != None:
            self.doCall(self.rootAgent.authMasterKey(masterKey=self.authMasterKey))
        elif self.authUser != None:
            import getpass
            self.userAgent = self.createRemoteAgent(1, UserAgent)
            
            password = self.authPassword
            if password is None:
                password = getpass.getpass()
            
            self.call(self.userAgent.authUser(userName=self.authUser, 
                                              userPassword=password),
                      self.gotUserResponse, self.gotError)
        else:
            self.ask()
    
    def _read_args(self):
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
            except IOError:
                interrupts += 1
                if interrupts == 2:
                    reactor.stop()
                    raise KeyboardInterrupt()
                
                print 'Interrupted'
                
                command = ''
                prompt = 'TS %s> ' % self.context.path()
                
        return args
    
    def ask(self):
        args = []
        
        while not self.context.async:
            args = self._read_args()
            
            if not args:
                continue
            
            while args:
                try:
                    self.context, args = self.context.call(args)
                except Exception as e:
                    print >> sys.stderr, str(e)
                    args = []
    
    def doCall(self, callId):
        self.call(callId, self.gotResponse, self.gotError)
    
    def gotResponse(self, response):
        self.context, args = self.context.doResponse(response)
        self.ask()
    
    def gotUserResponse(self, response):
        self.ask()
    
    def gotError(self, code, error):
        print >> sys.stderr, 'ERROR %d: %s' % (code, error)
        self.ask()
    
    def help(self, args):
        pass
    