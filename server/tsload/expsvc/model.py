'''
Created on Jun 11, 2013

@author: myaut
'''

from storm.locals import *
from storm.store import Store

from tsload.util.stormx import TableSchema

class Agent(object):
    id = Int(primary=True)
    
    uuid = UUID(allow_none=False)
    uuid.unique = True
    
    agentType = Enum(map = {'load': 0,
                            'monitor': 1})
    
    # Basic hostinfo information
    hostname = Unicode()
    domainname = Unicode()
    osname = RawStr()
    release = RawStr()
    machineArch = RawStr()
    
    numCPUs = Int()
    numCores = Int()
    memTotal = Int()
    
    # Registration information
    lastOnline = DateTime()
    
    