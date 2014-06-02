
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
