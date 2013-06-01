'''
Created on 09.05.2013

@author: myaut
'''

from tsload.jsonts.agent import TSAgent
from twisted.internet import reactor

server = TSAgent.createAgent('generic', 'localhost', 9090)
reactor.run()