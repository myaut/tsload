# TS test - unit testing for agent

import sys
import json

import shlex

from SCons.Errors import StopError 

class TestException(Exception):
    pass

class Test:
    EXPECT_OK     = 0
    EXPECT_SIGNAL = 1
    EXPECT_RETURN = 2
    EXPECT_TIMEOUT = 3
    
    SIGABRT = 6
    SIGNALS =  {'SEGV': 11}
    
    def __init__(self, name, group):
        self.bin = None
        self.name = name
        self.group = group
        self.groups = [group]
        
        self.maxtime = 1
        self.enabled = True
        self.expect = (Test.EXPECT_OK, 0)
        
        self.plat = 'generic'
        
        self.groupfiles = []
        self.files = []
        self.dirs = []
        
        self.libs = []
        self.mods = []
        self.uses = []
        self.subsystems = []
        
        self.extlibs = []
        
        self.args = ''
    
    def __str__(self):
        return '<tstest.Test %s/%s %s>' % (self.name, self.group, 
                                           self.files + self.dirs)
    
    def add_param(self, name, value):
        if name == 'file':
            self.files.append(value)
        elif name == 'groupfile':
            if value not in self.groupfiles:
                self.groupfiles.append(value)
        elif name == 'dir':
            self.dirs.append(value)
        elif name == 'extlib':
            try:
                plat, lib = value.split(':')
                self.extlibs.append((plat, lib))
            except ValueError:
                raise TestException('Invalid syntax for extlib=%s' % repr(value))
        elif name == 'use':
            self.uses.append(value)
        elif name == 'ss':
            self.subsystems.append(value)
        elif name == 'lib':
            self.libs.append(value)
        elif name == 'mod':
            self.mods.append(value)
        elif name == 'bin':
            self.bin = value
        elif name == 'args':
            self.args += ' ' + value            
        elif name == 'enabled':
            self.enabled = False if value == 'False' else True
        elif name == 'plat':
            self.plat = value
        elif name == 'maxtime':
            try:
                self.maxtime = int(value)
            except ValueError:
                raise TestException('Not an integer: %s in maxtime=...' % repr(value))
        elif name == 'expect':
            if value == 'ok':
                self.expect = (Test.EXPECT_OK, 0)
            elif value == 'abort':
                self.expect = (Test.EXPECT_SIGNAL, Test.SIGABRT)
            elif value == 'timeout':
                self.expect = (Test.EXPECT_TIMEOUT, 0)
            elif value.startswith('signal:'):
                signame = value.split(':', 2)[1]
                
                if signame not in Test.SIGNALS:
                    raise TestException('Invalid signal specified: %s' % repr(signame))
                
                self.expect = (Test.EXPECT_SIGNAL, Test.SIGNALS[signame])
            elif value.startswith('return:'):
                retcode = value.split(':', 2)[1]
                
                try:
                    retcode = int(retcode)
                except ValueError:
                    raise TestException('Not an integer: %s in expect=return:...' % repr(retcode))
                
                self.expect = (Test.EXPECT_RETURN, retcode)
            else:
                raise TestException('Invalid expect specified: %s. Should be ok, signal or return' % repr(value))
        else:
            raise TestException('Unknown param %s' % name)

class TestSuite:
    def __init__(self, test_f):
        self.tests = []
        self.group_params = {}
        
        self.parse(test_f)
    
    def __iter__(self):
        return iter(self.tests)
    
    def parse(self, test_f):
        wholeline = ''
        lnum = 0
        error = False
        
        for line in test_f:
            line = line.strip()
            lnum += 1
            
            # Ignore whitespace lines and comments
            if not line or line.startswith('#'):
                continue
            
            
            if line.endswith('\\'):
                line = line[:-1].strip()
                wholeline += line + ' '
                continue
            
            wholeline += line
            
            try:
                if wholeline.startswith('^'):
                    self.parse_group(wholeline[1:])
                else:
                    self.parse_test(wholeline)
                wholeline = ''
            except (TestException, ValueError) as e:
                error = True
                print >> sys.stderr, 'Error on line %d:' % lnum, str(e)
                print >> sys.stderr, '\t\t ' + wholeline
        
        if error:
            raise StopError('Failed to parse test suite config!')
    
    @staticmethod
    def parse_params(params):
        for param in params:
            if '=' not in param:
                raise TestException('Missing param value in %s!' % repr(param))
            name, value = param.split('=', 1)
            
            yield name, value
    
    def parse_group(self, line):
        params = shlex.split(line)
        group = params.pop(0)
        
        self.group_params[group] = list(self.parse_params(params))
    
    def copy_group_params(self, test, group):
        for name, value in self.group_params[group]:
            if name == 'extends':
                test.groups.append(value)
                self.copy_group_params(test, value)
                continue            
            
            test.add_param(name, value)
            
            if name == 'file':
                test.add_param('groupfile', value)
            if name == 'ss':
                test.add_param('groupfile', 'init.c')
    
    def parse_test(self, line):
        params = shlex.split(line)
        first = params.pop(0)
        
        if '/' not in first:
            raise TestException('Missing group name in %s!' % repr(first))
        group, name = first.split('/')
        
        if group not in self.group_params:
            raise TestException('Group %s is not defined' % repr(group))
        
        test = Test(name, group)
                
        self.copy_group_params(test, group)
        
        for name, value in self.parse_params(params):
            test.add_param(name, value)
        
        if not test.files and not test.dirs and test.bin is None:
            raise TestException('Invalid test %s: no source dirs or files was specified, and no destination binary was set!' % first)
        
        if test.bin is not None and (test.files or test.dirs):
            raise TestException('Invalid test %s: source dirs or files and destination binary are mutually exclusive!' % first)
        
        self.tests.append(test)