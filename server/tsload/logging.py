
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



# Ouch! We need import from python logging module, not ours module again
from __future__ import absolute_import

import os
import sys
import logging
import logging.handlers

import time

from tsload import config
from twisted.internet import reactor

_loggingHandlers = []
# _logFormat = "%(asctime)s %(levelname)s %(module)s:%(name)s | %(message)s"
_loggingLevels = {'DEBUG': logging.DEBUG,
                  'INFO': logging.INFO,
                  'WARN': logging.WARN}

loggingLevel = logging.DEBUG

_traceFile = None
_traceFacilities = ['server', 'agent', 'storm']

class Formatter(logging.Formatter):
    def formatTime(self, record, datefmt=None):
        return time.ctime(record.created)
    
    def format(self, record):
        # Make logging format conformant with libtscommon/log 
        levels = {logging.CRITICAL: "CRIT",
                  logging.ERROR: "ERR",  
                  logging.WARNING: "WARN", 
                  logging.INFO: "INFO", 
                  logging.DEBUG: "_DBG"}
        levelName = levels.get(record.levelno, 'NOTSET')
        
        msg = "%s [%s:%4s] %s" % (self.formatTime(record),
                                  record.name, 
                                  levelName,
                                  record.getMessage())
        return msg

def _stopLogging():
    global _traceFile
    
    if _traceFile is not None:
        _traceFile.close() 
        
    logging.shutdown()

def initBasicLogging():
    global _traceFile
    
    logging.basicConfig(format = '%(asctime)s [%(name)s:%(levelname)s] %(message)s',
                        stream = sys.stderr)
    _traceFile = sys.stderr
    
    reactor.addSystemEventTrigger('before', 'shutdown', 
                                  logging.shutdown)

def initLogging():    
    global loggingLevel, _traceFile
    
    logLevel =  config.get('logging', 'level')
    if logLevel:
        loggingLevel = _loggingLevels[logLevel]    
    
    if config.getBoolean('logging', 'stderr'):
        _loggingHandlers.append(logging.StreamHandler(sys.stderr))
        
    if config.getBoolean('logging', 'stdout'):
        _loggingHandlers.append(logging.StreamHandler(sys.stdout))
     
    logFileName = config.get('logging', 'logfile')    
    if logFileName:
        handler = logging.handlers.RotatingFileHandler(logFileName, 
                                                       maxBytes = 2 * 1024 * 1024,
                                                       backupCount = 5)
        _loggingHandlers.append(handler)
    
    # Initialize tracing
    traceFileName =  config.get('logging', 'tracefile')  
    if traceFileName:
        if os.path.exists(traceFileName):
            os.rename(traceFileName, traceFileName + '.old')
        
        _traceFile = file(traceFileName, 'w')
        
        try:
            from tsload.util.stormx import initStormTracing
            initStormTracing()
        except ImportError:
            # Storm is not installed, this is OK
            pass
    
    formatter = Formatter()
    for handler in _loggingHandlers:
        handler.setFormatter(formatter) 
    
    reactor.addSystemEventTrigger('before', 'shutdown', 
                                  _stopLogging)
    
def getLogger(name):
    global loggingLevel
    
    logger = logging.getLogger(name)
    
    logger.setLevel(loggingLevel)
    for handler in _loggingHandlers:
        logger.addHandler(handler)
        
    return logger

def trace(facility, msg, *args, **kwargs):
    global _traceFile, _traceFacilities
    if _traceFile is not None and facility in _traceFacilities:
        t = time.ctime(time.time())
        
        if isinstance(kwargs, dict) and kwargs:
            msg = msg % kwargs
        else:
            msg = msg % args
        
        msg = "%s [%s:_TRC] %s\n" % (t, facility, msg)
        
        _traceFile.write(msg)
        _traceFile.flush()
