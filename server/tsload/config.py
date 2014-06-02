
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



import os
from ConfigParser import ConfigParser

config = None

# FIXME: Must be cross-platform
globalPath = '/etc'

def _getServerPath():
    return os.path.dirname(os.path.abspath(__file__))

def readConfig(filename):
    '''Reads config from file named filename.
    Searches file in <tsload>/../etc and global configuration directory
    (/etc on Unix-like systems)'''
    global config
    
    # serverPath returns path to tsload! Be careful in case of refactoring
    serverPath = _getServerPath()
    etcPath = os.path.normpath(os.path.join(serverPath, '..', 'etc'))
    
    config = ConfigParser()
    cfgpaths = [os.path.join(etcPath, filename),
                os.path.join(globalPath, filename)]
    config.read(cfgpaths)
    
    return config

def get(section, option):
    global config
    return config.get(section, option)

def getInt(section, option):
    global config
    return config.getint(section, option)

def getBoolean(section, option):
    global config
    return config.getboolean(section, option)

def getFloat(section, option):
    global config
    return config.getfloat(section, option)

def getPath(section, option):
    global config
    
    serverPath = _getServerPath()
    path = config.get(section, option)
    
    if os.path.isabs(path):
        return path
    
    path = os.path.normpath(os.path.join(serverPath, '..', path))
    return path

def setWorkDir(section):
    '''Sets working directory using global config
    
    @param section - section from config that contains it'''
    wdPath = getPath(section, 'workdir')
    
    os.chdir(wdPath)

