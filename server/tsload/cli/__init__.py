
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



import os
import sys
import shlex

import socket
import traceback

from tsload.jsonts import JSONTS
from tsload.jsonts.agent import TSAgent

from tsload.cli.context import CLIContext, NextContext, SameContext
from tsload.cli.agent import AgentRootContext
from tsload.cli.profile import ProfileRootContext

from tsload.jsonts.api.user import UserAgent
from tsload.jsonts.api.expsvc import ExpSvcAgent

from twisted.internet import reactor
from twisted.internet.defer import Deferred, inlineCallbacks

import signal

try:
    import readline
    import atexit
    
    useReadline = True
except ImportError:
    useReadline = False

# TODO: server disconnection handling

class RootContext(CLIContext):
    name = ''
    operations = ['agent', 'profile']
    
    @NextContext(AgentRootContext)
    def agent(self, args):
        '''Agent administration'''
        return args
    
    @NextContext(ProfileRootContext)
    def profile(self, args):
        '''Experiment profiles'''
        return args
    
    @SameContext()
    def doResponse(self, response):
        pass

class SigIntHandler:
    def __enter__(self):
        # Twisted reactor installs it's own SIGINT handler, which breaks _readArgs logic
        # It can be overriden by installSignalHandlers=False, but we loose SIGTERM etc.
        # Manually reset to SIGINT which cause sys.stdin.readline() throw KeyboardInterrupt()
        # NOTE: should be called after reactor is initialized, from it's context
        self._sigintHandler = signal.signal(signal.SIGINT, signal.default_int_handler)
    
    def __exit__(self, *args):
        signal.signal(signal.SIGINT, self._sigintHandler)

class TSAdminCLIAgent(TSAgent):
    _readLineHistoryFile = '~/.tscli_history'
    
    def __init__(self):
        if useReadline:
            self._initReadLine()
        
        self._setContext(RootContext(None, self))
    
    def setAuthType(self, authUser, authMasterKey, authPassword=None):
        self.authUser = authUser
        self.authPassword = authPassword
        self.authMasterKey = authMasterKey
    
    @inlineCallbacks
    def gotAgent(self):
        self.expsvcAgent = self.createRemoteAgent(2, ExpSvcAgent)
        
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
    
    def _setContext(self, ctx):
        self.context = ctx
        
        if useReadline:
            readline.set_completer(self._completerProxy) 
    
    def _readLine(self, prompt):
        sys.stdout.write(prompt)
        sys.stdout.flush()
        
        return sys.stdin.readline()
    
    def _readLineModule(self, prompt):
        return raw_input(prompt)
    
    def _completerProxy(self, text, state):
        text = readline.get_line_buffer()
        return self.context.completer(text, state)
    
    def _initReadLine(self):
        readline.parse_and_bind("tab: complete")
        
        self._readLineHistoryFile = os.path.expanduser(self._readLineHistoryFile)
        
        if hasattr(readline, "read_history_file"):
            try:
                readline.read_history_file(self._readLineHistoryFile)
            except IOError:
                pass
            atexit.register(self._saveReadLine, None)
        self._readLine = self._readLineModule
    
    def _saveReadLine(self, *args):
        readline.write_history_file(self._readLineHistoryFile)
    
    def _readArgs(self):
        prompt = 'TS %s> ' % self.context.path()
        command = ''
        done = False
        
        interrupts = 0
        
        sys.stdout.flush()
        
        while not done:
            try:
                with SigIntHandler():
                    input = self._readLine(prompt)
                
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
        '''Execute command implementation
        
        There are two path for asynchronous server-side commands:
        
        1. Using of synchronous context (caller) and async (callee):
            -> Caller uses @NextContext(callee) and returns deferred (i.e. it 
               is returned by TSMethodImpl from agent interfaces)
            -> self.context set to callee, which has async flag set
            -> _executeImpl sets last callback/errback to gotResponse/gotError callbacks
               and returns control to reactor
            -> Server/Agents execute command and returning results; gotResponse firing,
               and ask() called from reactor, reading new command from terminal
        2. Returning deferred directly from command call'''
        origContext = self.context
        
        # Helpers for deferreds
        def gotResponse(response):
            ctx, _ = self.context.doResponse(response)
            self._setContext(ctx)
            
            reactor.callLater(0, self.ask)
            
        def gotError(failure):
            self.context.async = self.context.oldAsync
            
            self._setContext(origContext) 
            
            print >> sys.stderr, 'ERROR %s' % failure
            reactor.callLater(0, self.ask)
        
        while not self.context.async and args:
            self.context, d = self.context.call(args)
            
            if isinstance(d, Deferred):
                # Synchronous context entered asynchronous operation (such as inline 
                # server call). Temporarily set async flag, which is reset on doResponse path
                self.context.oldAsync = self.context.async
                self.context.async = True
        
        if isinstance(d, Deferred):
            # Last callbacks and errbacks is our gotResponse and gotError,
            # which are calling ask() from reactors context
            d.addCallback(gotResponse)
            d.addErrback(gotError)
        else:
            reactor.callLater(0, self.ask)
    
    def _execute(self, args):
        '''Exception-safe wrapper for _executeImpl'''
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
    
