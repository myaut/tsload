'''
Created on 09.05.2013

@author: myaut
'''

from tsload import config, logging

config.readConfig('server.cfg')
config.setWorkDir('tsserver')
logging.initLogging()

from tsload.jsonts.server import TSServer
from tsload.jsonts.root import TSRootAgent
from tsload.user import TSUserAgent
from tsload.expsvc import TSExperimentSvcAgent

from twisted.internet import reactor

port = config.getInt('tsserver', 'port')
logging.getLogger('__main__').info('Starting TSLoad server instance at port %d...' % port)
server = TSServer.createServer(port)

server.createLocalAgent(TSRootAgent, 0)
server.createLocalAgent(TSUserAgent, 1, config.get('tsserver:user', 'database'))
server.createLocalAgent(TSExperimentSvcAgent, 2, config.get('tsserver:expsvc', 'database'))

reactor.run()