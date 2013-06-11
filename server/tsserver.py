'''
Created on 09.05.2013

@author: myaut
'''

from tsload.jsonts.server import TSServer
from tsload.jsonts.root import TSRootAgent
from tsload.user import TSUserAgent
from tsload.expsvc import TSExperimentSvcAgent

from twisted.internet import reactor

server = TSServer.createServer(9090)
server.createLocalAgent(TSRootAgent, 0)
server.createLocalAgent(TSUserAgent, 1, 'sqlite:user.db')
server.createLocalAgent(TSExperimentSvcAgent, 2, 'sqlite:expsvc.db')

reactor.run()