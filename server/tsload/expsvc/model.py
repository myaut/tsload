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

class WorkloadType(object):
    __storm_table__ = 'agent_workload_type'
    
    id = Int(primary = True)
    
    aid = Int()
    
    agent = Reference(aid, Agent.id)
    
    name = Unicode()
    
    module = RawStr()
    modulePath = RawStr()
    
    classList = RawStr()
    
class WorkloadParam(object):
    __storm_table__ = 'agent_workload_param'
    
    id = Int(primary = True)
    
    wltid = Int()
    workloadType = Reference(wltid, WorkloadType.id)
    
    name = Unicode()    
    paramData = JSON()

class ExperimentProfile(object):
    __storm_table__ = 'experiment_profile'
    
    id = Int(primary = True)
    
    # This field references user id inside user database
    userId = Int()
    
    name = Unicode()
    description = Unicode()
    
    creationDate = Date()

class ExperimentThreadPool(object):
    __storm_table__ = 'experiment_threadpool'
    
    id = Int(primary = True)
    
    aid = Int()
    agent = Reference(aid, Agent.id)
    
    eid = Int()
    experimentProfile = Reference(eid, ExperimentProfile.id)
    
    name = Unicode()    
    numWorkers = Int()
    
    # TODO: Threadpool binding API

class ExperimentWorkload(object):
    __storm_table__ = 'experiment_workload'
    
    id = Int(primary = True)
    
    eid = Int()
    experimentProfile = Reference(eid, ExperimentProfile.id)
    
    tpid = Int()
    threadpool = Reference(tpid, ExperimentThreadPool)
    
    wltid = Int()
    workloadType = Reference(wltid, WorkloadType.id)
    
    name = Unicode()
    params = JSON()
    
    stepsId = RawStr()  # References to TSDB 

class ExperimentWorkloadResource(object):
    __storm_table__ = 'experiment_workload_resource'
    
    id = Int(primary = True)
    
    wid = Int()
    workload = Reference(wid, ExperimentWorkload.id)
    
    rid = Int()
    resource = Reference(rid, AgentResource.id)
    
    rmode = Int()

def createExpsvcDB(connString):
    database = create_database(connString)
    store = Store(database)
    
    TableSchema(database, Agent).create(store)
    TableSchema(database, AgentResource).create(store)
    TableSchema(database, AgentResourceChild).create(store)
    TableSchema(database, WorkloadType).create(store)
    TableSchema(database, ExperimentProfile).create(store)
    TableSchema(database, ExperimentThreadPool).create(store)
    TableSchema(database, ExperimentWorkload).create(store)
    TableSchema(database, ExperimentWorkloadResource).create(store)
    
    store.commit()
    store.close()