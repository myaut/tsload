from tsload import config

from tsload.user.model import createUserDB
from tsload.expsvc.model import createExpsvcDB

import os

config.readConfig('server.cfg')
config.setWorkDir('tsserver')

os.mkdir('db')
os.mkdir('log')

def initializeDB(initializer, section, option):
    db = config.get(section, option)
    initializer(db)
    print 'Database at ' + db + ' was initialized'
    
initializeDB(createUserDB, 'tsserver:user', 'database')
initializeDB(createExpsvcDB, 'tsserver:expsvc', 'database')
