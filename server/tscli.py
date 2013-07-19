#!/usr/bin/python 

import sys
import getopt

from tsload import logging

from tsload.cli import TSAdminCLIAgent

from twisted.internet import reactor

usageStr = '''tscli.py [-h host] [-p port] {-u user [-P password]|-m master-key}'''

def usage(error):
    print >> sys.stderr, error
    print >> sys.stderr, usageStr
    sys.exit(2) 

HOST = 'localhost'
PORT = 9090

logging.initBasicLogging()

authUser = None
authPassword = None
authMasterKey = None

try:
    opts, args = getopt.getopt(sys.argv[1:], 's:p:u:P:m:',
                               ['server=', 'port=', 'user=', 'password=', 'master='])
except getopt.GetoptError as goe:
    usage(str(goe))

for opt, optarg in opts:
    if opt in ['-s', '--server']:
        HOST = optarg
    elif opt in ['-p', '--port']:
        PORT = int(optarg)
    elif opt in ['-u', '--user']:
        authUser = optarg
    elif opt in ['-P', '--password']:
        authPassword = optarg
    elif opt in ['-m', '--master']:
        authMasterKey = optarg
    else:
        usage('Unknown option %s' % opt)
        
if authUser is None and authMasterKey is None:
    usage('Choose authentification method')

if authUser is not None and authMasterKey is not None:
    usage('-u and -m are mutually exclusive')

agent = TSAdminCLIAgent.createAgent('admin', HOST, PORT)
agent.setAuthType(authUser, authMasterKey, authPassword)

reactor.run()