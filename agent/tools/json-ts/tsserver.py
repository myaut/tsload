#!/usr/bin/python

import sys

import SocketServer
import time

from threading import Thread
from Queue import Queue

import logging
from logging import StreamHandler, Formatter

import time
import random

import traceback

from jsonts import *

MAX_STEPS = 5
MAX_REQUESTS = 20

PORT = 9090

MILLISECOND = 1000 * 1000
SECOND = 1000 * 1000 * 1000

WORKLOAD_SIMPLEIO = {'wltype': 'simpleio_read',
                     'threadpool': 'test',
                     'params': {'filesize': 134217728,
                                'blocksize': 4096,
                                'path': 'C:\\testfile',
                                'sparse': False,
                                'sync': True}}
WORKLOAD_BUSY_WAIT = { 'wltype': 'busy_wait',
                       'threadpool': 'test',
                       'params': {'num_cycles': 400000 } }

class WorkloadConfigurator(Thread):
    finished = False
    daemon = True
    
    def __init__(self):
        self.queue = Queue()
        
        Thread.__init__(self)
    
    def run(self):
        while True:
            client = self.queue.get()
            
            if client is None:
                break
            
            try:
                self.configureWorkload(client)
            except:
                print >> sys.stderr, '-' * 20
                traceback.print_exc(file = sys.stderr)
                print >> sys.stderr, '-' * 20
    
    def configureWorkload(self, client):
        workload = WORKLOAD_BUSY_WAIT
        
        client.invoke('create_threadpool',
                      tp_name='test',
                      num_threads=2,
                      quantum = 250 * MILLISECOND,
                      disp_name='simple' )
        
        client.invoke('configure_workload', 
                      workload_name='test', 
                      workload_params=workload)


class WorkloadRunner(Thread):
    finished = False
    daemon = True
    
    logger = logging.getLogger('Runner')
    logger.setLevel(logging.DEBUG)
    
    def __init__(self, client):
        self.client = client
        self.step = 0
        
        Thread.__init__(self)
        
    def run(self):
        start_time = int((time.time() + 3) * SECOND)
        
        self.logger.info('Scheduling workload test on %d' % start_time)
        
        self.client.invoke('start_workload',
                           workload_name = 'test', 
                           start_time = start_time)
        
        print self.client.invoke('get_threadpools')
                
        while self.step < MAX_STEPS:
            numRequests = random.randint(0, MAX_REQUESTS)
            provideStepResult = { 'success': False }
            
            #POLL
            while not provideStepResult['success']:
                provideStepResult = self.client.invoke('provide_step', 
                                                       workload_name = 'test', 
                                                       step_id = self.step,
                                                       num_requests = numRequests)
            
            self.step += 1

class TSLoadServer(TSServer):
    logger = logging.getLogger('Server')
    logger.setLevel(logging.DEBUG)
    
    def processClient(self, client):
        global wlConfigThread
        wlConfigThread.queue.put(client)
    
    def processCommand(self, client, cmd, msg):
        for attr in dir(self):
            method = getattr(self, attr)
            
            if not callable(method):
                continue
            
            if 'cmdName' not in dir(method):
                continue
            
            if method.cmdName == cmd:
                return method(client, **msg)
        else:
            raise TSException('Operation is not supported')
    
    def workloadStatus(self, client, workload_name, status, progress, message):
        self.logger.info('Workload %s status %d(%d): %s' % (workload_name,
                                                            status,
                                                            progress,
                                                            message))
        
        # SUCCESS
        if status == 2:
            global wlRunners
            
            wlRunner = WorkloadRunner(client)
            wlRunner.start()
            
            wlRunners.append(wlRunner)
        
    workloadStatus.cmdName = 'workload_status'
    
    def requestsReport(self, client, requests):
        self.logger.info('Requests report: ')
        
        for rq in requests['requests']:
            self.logger.info('\t%(step)05d %(request)05d %(thread)02d %(start)d...%(end)d %(flags)02x' % rq)
    requestsReport.cmdName = 'requests_report'
                
logging.basicConfig(format='%(asctime)s  |  %(message)s')

wlConfigThread = WorkloadConfigurator()
wlConfigThread.start()

wlRunners = []

server = TSLoadServer(("0.0.0.0", PORT), TSRequestHandler)
server.serve_forever()