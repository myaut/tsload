'''
Created on Jun 11, 2013

@author: myaut
'''

from storm.locals import *
from storm.store import Store

from tsload.util.stormx import TableSchema, UniqueConstraint

class Agent(object):
    __storm_table__ = 'agent'
    
    id = Int(primary=True)
    
    uuid = UUID(allow_none=True)
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

class AgentResource(object):    
    __storm_table__ = 'agent_resource'
    
    id = Int(primary=True)
    
    aid = Int()
    aid.index = True
    
    agent = Reference(aid, Agent.id)
    
    resourceClass = Enum(map = {'cpu': 0,
                                'disk': 1,
                                'fs': 2,
                                'net': 3})
    
    name = Unicode()
    resourceType = RawStr()
    data = JSON()
    
    state = Int()

class AgentResourceChild(object):
    __storm_table__ = 'agent_resource_child'
    
    id = Int(primary = True)
    
    pid = Int()
    cid = Int()
    
    parent = Reference(pid, AgentResource.id)
    child  = Reference(cid, AgentResource.id)
    
    uniqParentChild = UniqueConstraint(pid, cid)

def createExpsvcDB(connString):
    database = create_database(connString)
    store = Store(database)
    
    TableSchema(database, Agent).create(store)
    TableSchema(database, AgentResource).create(store)
    TableSchema(database, AgentResourceChild).create(store)
    
    store.commit()
    store.close()