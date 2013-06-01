from storm.locals import *
from storm.store import Store

from tsload.util.stormx import TableSchema

class User(object):
    __storm_table__ = "user"
    
    id = Int(primary=True)
    
    name = RawStr()
    name.unique = True
    
    gecosName = Unicode()
    
    # Auth service used to authentificate user
    authService = Enum(map = {'local': 0
                              })
    authPassword = RawStr()

class Role(object):
    __storm_table__ = "role"
    
    id = Int(primary=True)
    
    # Reference to user
    user_id = Int()
    user = Reference(user_id, User.id)
    
    role = Enum(map = {'admin': 0,
                       'operator': 1,
                       'user': 2})
    
    # Agent's uuid for operator/user roles
    # For admin role this is optional, because admin user
    # may access any agent
    agentUuid = UUID(allow_none=True)                  
    
User.roles = ReferenceSet(User.id, Role.user_id)

class UserAuthError(Exception):
    pass

def createUserDB(connString):
    '''Creates user database and default admin account 
    with password admin'''
    from tsload.user.localauth import LocalAuth
    
    database = create_database(connString)
    store = Store(database)
    
    TableSchema(database, User).create(store)
    TableSchema(database, Role).create(store)
    
    localAuth = LocalAuth()
    
    admin = User()
    
    admin.name = 'admin'
    admin.gecosName = u'TSLoad Administrator'
    admin.authService = 'local'
    
    localAuth.changePassword(admin, 'admin')
    
    store.add(admin)
    
    adminRole = Role()
    adminRole.user = admin
    adminRole.role = 'admin'
    
    store.add(adminRole)
    
    store.commit()
    
    store.close()