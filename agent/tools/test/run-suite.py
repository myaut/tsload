import os

import sys
import traceback

import time

from Queue import Queue
from threading import Thread, Timer, Lock
from subprocess import Popen, PIPE
from ConfigParser import ConfigParser

import tempfile
import shutil

class TestError(Exception):
    pass

class Test:
    EXPECT_OK     = 0
    EXPECT_SIGNAL = 1
    EXPECT_RETURN = 2
    EXPECT_TIMEOUT = 3
        
    pass

class TestRunner(Thread):
    daemon = True
    test_dir = None
    
    def __init__(self, suite, tid):
        self.suite = suite
        self.tid = tid
        
        self.test_dir = os.path.join(suite.test_dir, 'test-%d' % self.tid) 
        
        name = 'TestRunner-%d' % self.tid
        Thread.__init__(self, name = name)
    
    def __del__(self):
        if self.test_dir is not None:
            self.wipe_test_dir()
    
    def read_test_config(self, test_name):
        test = Test()
        
        cfg = ConfigParser()
        
        cfg.read(test_name + '.cfg')
        
        test.name = cfg.get('test', 'name')
        test.group = cfg.get('test', 'group')
        
        test.maxtime = cfg.getint('test', 'maxtime')
        test.expect = cfg.getint('test', 'expect')
        test.expect_arg = cfg.getint('test', 'expect_arg')
        
        test.libs = [v for n, v in cfg.items('libs')]
        test.uses = [v for n, v in cfg.items('uses')]
        
        return test
    
    def stop_test(self, proc):
        proc.timed_out = True
        proc.kill()
    
    def analyze_result(self, test, proc):
        ''' Returns True if proc result was expected '''
        ret = proc.returncode
        
        if test.expect == Test.EXPECT_OK:
            return ret == 0
        elif test.expect == Test.EXPECT_SIGNAL:
            return ret < 0 and -ret == test.expect_arg
        elif test.expect == Test.EXPECT_RETURN:
            return ret == test.expect_arg
        elif test.expect == Test.EXPECT_TIMEOUT:
            return proc.timed_out
        
        return False   
    
    def prepare_test_dir(self, test, test_name):
        os.mkdir(self.test_dir)
        
        for fn in [test_name] + test.libs + test.uses:
            if fn.startswith('#'):
                fn = fn[1:]
            
            base_name = os.path.basename(fn)
            
            if os.path.isdir(fn):
                shutil.copytree(fn, os.path.join(self.test_dir, base_name))
            else:
                shutil.copy(fn, os.path.join(self.test_dir, base_name))
    
    def wipe_test_dir(self):
        shutil.rmtree(self.test_dir)
        
    def check_core(self, test_name, test_group):
        core_path = os.path.join(self.test_dir, 'core')
        
        if os.path.exists(core_path):
            core_name = 'core_' + test_name.replace(os.sep, '_')
            
            if sys.platform == 'win32':
                core_name += '.dmp'
                dest_dir = os.path.join('build', 'test', test_group)
            else:
                dest_dir = os.path.join('build', 'test')
            
            shutil.copy(core_path, os.path.join(dest_dir, core_name))
            return True
        
        return False
    
    def run_test(self, test_name):
        test = self.read_test_config(test_name)
        test_bin = os.path.basename(test_name)
        test_env = os.environ.copy()
        
        self.prepare_test_dir(test, test_name)
        
        if sys.platform in ['linux2', 'sunos5']:
            test_env['LD_LIBRARY_PATH'] = '.'
        
        start = time.time()
        proc = Popen(os.path.join(self.test_dir, test_bin), 
                     stdout = PIPE,
                     stderr = PIPE,
                     cwd = self.test_dir,
                     env = test_env)
        proc.timed_out = False
        
        timer = Timer(test.maxtime, self.stop_test, args = [proc])
        timer.start()
        
        proc.wait()
        timer.cancel()
        
        end = time.time()
        
        core = self.check_core(test_name, test.group)
        
        self.wipe_test_dir()
        
        result = self.analyze_result(test, proc)
        
        stdout = proc.stdout.read()
        stderr = proc.stderr.read()
        
        output = '' 
        if proc.timed_out:
            output += 'Timed out\n'
        if core:
            output += 'Dumped core\n'
        output += 'WD: %s Name: %s \n' % (self.test_dir, test_bin)
        output += 'Time: %f\n\n' % (end - start)
        if stdout:
            output += '=> stdout: \n'
            output += stdout
        if stderr:
            output += '=> stderr: \n'
            output += stderr
        
        self.suite.report_test('%s/%s' % (test.group, test.name),
                               result, proc.returncode,
                               output)
    
    def run(self):
        while True:
            test_name = self.suite.get()
            
            try:
                self.run_test(test_name)
            except Exception:
                self.suite.report_exc(test_name, sys.exc_info())
            
            self.suite.done()

class TestSuite:
    def __init__(self):
        self.queue = Queue()
        
        self.ok_count = 0
        self.failed_count = 0
        self.error_count = 0
        
        self.report_lock = Lock()
        
        self.runner_count = int(os.getenv('TSTEST_RUNNER_THREADS', 1))
        self.create_test_dir()
        
        self.runners = []
        for tid in range(self.runner_count):
            thread = TestRunner(self, tid)
            self.runners.append(thread)
            
            thread.start() 
    
    def check_test_dir(self):
        if os.listdir(self.test_dir):
            raise TestError('Test directory %s is not empty; exiting' % self.test_dir)
    
    def create_test_dir(self):
        self.test_dir = os.getenv('TSTEST_TEST_DIRECTORY')
        self.test_dir_delete = False
        
        if not self.test_dir:
            # Reset tempdir
            self.test_dir = tempfile.mkdtemp(prefix='tstest')
        else:
            if not os.path.isdir(self.test_dir):
                raise TestError('Test directory %s is not a directory' % self.test_dir)
        
        if not os.path.exists(self.test_dir):
            os.mkdir(self.test_dir)    
            self.test_dir_delete = True
            
        self.check_test_dir()
    
    def delete_test_dir(self):
        self.check_test_dir()
        
        if self.test_dir_delete:
            os.rmdir(self.test_dir)   
    
    def get(self):
        return self.queue.get()
    
    def done(self):
        self.queue.task_done()
        
    def report_test(self, test_name, result, retcode, output):
        if result:
            self.ok_count += 1
            resstr = 'OK'
        else:
            self.failed_count += 1
            resstr = 'FAILED'
        
        with self.report_lock:
            print '=== TEST: %-20s RESULT: %-8s RETCODE: %d ===' % (test_name, resstr, retcode)
            print output
            
            # Print result to terminal
            print >> sys.stderr, 'Running test %s... %s' % (test_name, resstr)
    
    def report_exc(self, test_name, exc_info):
        self.error_count += 1
        
        with self.report_lock:
            info = traceback.format_exception(*exc_info)
            
            print '=== TEST: %-20s RESULT: %-8s ===' % (test_name, 'ERROR')
            print ''.join(info)
            
            # Print result to terminal
            print >> sys.stderr, 'Running test %s... ERROR' % (test_name)
    
    def run(self):
        for test_name in sys.argv[1:]:
            self.queue.put(test_name)
            
        self.queue.join()
        
        self.delete_test_dir()
        
        print '=== RESULTS: %d passed %d failed %d error ===' % (self.ok_count,
                                                                 self.failed_count, 
                                                                 self.error_count)

suite = TestSuite()
suite.run()    

sys.exit(suite.failed_count)