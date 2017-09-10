import os

import sys
import traceback

import time

from Queue import Queue
from threading import Thread, Timer, Lock
from subprocess import Popen, PIPE
from ConfigParser import ConfigParser

try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO

try:
    import cgi
    html_escape = cgi.escape
except Exception:
    html_escape = lambda text: text.replace('<', '&lt;').replace('>', '&gt;').replace('&', '&amp;')

try:
    from collections import OrderedDict
except ImportError:
    sys.path.append(os.path.join("tools", "build"))
    from ordereddict import OrderedDict

import tempfile
import shutil

import errno

class TestError(Exception):
    pass

class Test(object):
    EXPECT_OK     = 0
    EXPECT_SIGNAL = 1
    EXPECT_RETURN = 2
    EXPECT_TIMEOUT = 3
        
    pass

class Formatter(object):
    header = ''
    tailer = ''
    
    def __init__(self, outf):
        self.outf = outf
    
    def write(self, s):
        self.outf.write(s)
    
    def flush(self):
        self.outf.flush()

class PlainFormatter(Formatter):        
    def report_test(self, test_name, resstr, retcode = None):
        if retcode is None:
            fmtstr = '=== TEST: %-20s RESULT: %-8s ===\n'
            params = (test_name, resstr)
        else:
            fmtstr = '=== TEST: %-20s RESULT: %-8s RETCODE: %d ===\n'
            params = (test_name, resstr, retcode)
        self.outf.write(fmtstr % params)
        
    def report_results(self, ok_count, failed_count, error_count):
        fmtstr = '=== RESULTS: %d passed %d failed %d error ===\n'        
        self.outf.write(fmtstr % (ok_count, failed_count, error_count))
    
    def report_param(self, name, value):
        self.outf.write('%s: %s\n' % (name, value))
    
    def report_line(self, line):
        self.outf.write(line + '\n')
    
    def report_block(self, name, block):
        if sys.platform == 'win32':
            block = block.replace('\r', '')
        
        self.outf.write('\n<= %s\n' % name)
        self.outf.write(block)
        self.outf.write('<= %s\n\n' % name)
    
class HTMLFormatter(Formatter):
    header = '''
<HTML>
<HEAD>
    <TITLE>TSTest results</TITLE>
    <LINK rel="stylesheet" href="bootstrap.min.css">
</HEAD>
<BODY>
    '''
    
    tailer = '''
</BODY>
</HTML>
    '''   
    
    TEST_RESULT_CLASSES = {
        'OK': 'label-success',
        'ERROR': 'label-inverse',
        'FAILED': 'label-important'
    }
    
    def report_test(self, test_name, resstr, retcode = None):
        code = '<HR />\n<SPAN class="label %s">%s</SPAN><H3>%s</H3>\n'
        result_class = HTMLFormatter.TEST_RESULT_CLASSES[resstr]
        
        self.outf.write(code % (result_class, resstr, test_name))
        if retcode is not None:
            self.outf.write('<H4>Return code: %s</H4>\n' % retcode)
        self.outf.write('<BR /><BR />\n')
    
    def report_param(self, name, value):
        self.outf.write('<STRONG>%s</STRONG>: %s<BR/>\n' % (name, value))
    
    def report_line(self, line):
        self.outf.write(line + '<BR />\n')
        
    def report_block(self, name, block):
        if sys.platform == 'win32':
            block = block.replace('\r', '')
        
        self.outf.write(name)
        self.outf.write('<BR />\n<PRE>')
        self.outf.write(html_escape(block))
        self.outf.write('</PRE>\n')
    
    def report_results(self, ok_count, failed_count, error_count):
        self.outf.write('<H2>Results</H2><BR />\n')
        self.outf.write('<SPAN class="badge badge-success">%d</SPAN> ok<BR />\n' % ok_count)
        self.outf.write('<SPAN class="badge badge-important">%d</SPAN> failed<BR />\n' % failed_count)
        self.outf.write('<SPAN class="badge badge-warning">%d</SPAN> failed<BR />\n' % error_count)

DefaultFormatter = HTMLFormatter if os.getenv('TSTEST_USE_HTML', False)   \
                        else PlainFormatter

class TestRunner(Thread):
    daemon = True
    test_dir = None
    
    def __init__(self, suite, tid):
        self.suite = suite
        self.tid = tid
        
        self.test_dir = os.path.join(suite.test_dir, 'test-%d' % self.tid) 
        self.mod_dir = os.path.join(self.test_dir, 'mod')
        
        if sys.platform == 'win32':
            self.lib_subdir = ''
            self.bin_dir = self.lib_dir = self.test_dir
        else:
            self.lib_subdir = 'lib'
            self.lib_dir = os.path.join(self.test_dir, 'lib')
            self.bin_dir = os.path.join(self.test_dir, 'bin')
        
        self.log_name = 'tsload.log'
        
        name = 'TestRunner-%d' % self.tid
        Thread.__init__(self, name = name)
    
    def __del__(self):
        if self.test_dir is not None:
            self.wipe_test_dir()
    
    def read_uses(self, cfg):
        '''For use-files additional arguments may be passed
        in this case filename is a tuple where second element
        is an dict with parameters 'chmod' and 'rename' ''' 
        uses = []
        
        for use_name, use in cfg.items('uses'):
            if cfg.has_section(use_name):
                use = (use, dict(cfg.items(use_name)))
            uses.append(use)
        
        return uses
    
    def read_test_config(self, cfg_name):
        ''' Parses config generated by test/SConscript and create an
            Test object from it. It has the same attributes as tstest.Test
        '''
        test = Test()
        
        cfg = ConfigParser(dict_type=OrderedDict)
        
        cfg.read(cfg_name)
        
        test.name = cfg.get('test', 'name')
        test.group = cfg.get('test', 'group')
        
        test.maxtime = cfg.getint('test', 'maxtime')
        test.expect = cfg.getint('test', 'expect')
        test.expect_arg = cfg.getint('test', 'expect_arg')
        
        test.is_dist = cfg.getboolean('test', 'is_dist')
        
        # Binaries are parsed as-is
        test.libs = [v for n, v in cfg.items('libs')]
        test.mods = [v for n, v in cfg.items('mods')]
        test.bins = [v for n, v in cfg.items('bins')]
        
        test.uses = self.read_uses(cfg)
        
        test.args = cfg.get('test', 'args')                \
                        if cfg.has_option('test', 'args')  \
                        else None
        
        if len(test.bins) > 1:
            raise TestError('More than one binary is specified!')
        
        if len(test.bins) == 0:
            raise TestError('No test binary was specified!')
        
        return test
    
    def analyze_result(self, test, proc):
        ''' Returns True if proc result was expected (as 
            set by expect parameter, by default it is ok)'''
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
    
    def copy_test_files(self, dest_dir, files, log_copy = False):
        ''' Copies files for test. Helper for prepare_test_dir'''
        for fn in files:
            try:
                self._copy_test_file(dest_dir, fn, log_copy)
            except Exception as e:
                raise TestError("Failed processing copy-directive '%s'" % (fn, ), e)
    
    def _copy_test_file(self, dest_dir, fn, log_copy):
        chmod = None
        
        # See above in read_uses
        args = {}
        if isinstance(fn, tuple):
            fn, args = fn 
        
        # Cut-out SCons root-dir marking
        if fn.startswith('#'):
            fn = fn[1:]
                    
        dest_name = args.get('rename', os.path.basename(fn))
        dest_path = os.path.join(dest_dir, dest_name)        
        
        if args.get('remove') is None:
            if os.path.isdir(fn):
                shutil.copytree(fn, dest_path)
            else:
                shutil.copy(fn, dest_path)
                
            chmod = args.get('chmod')
            if chmod is not None:
                mode = int(chmod, 8)
                os.chmod(dest_path, mode)
        else:
            os.remove(dest_path)
        
        if log_copy:
            self.copy_log += '%s -> %s chmod:%s rm:%s\n' % (fn, dest_path, chmod, args.get('remove'))
    
    def setup_versioned_libs(self, dest_dir, libs):
        if sys.platform == 'win32':
            return
        
        for libfn in libs:
            libname = os.path.basename(libfn)
            liblink = libname
            
            while not liblink.endswith('.so') and liblink.count('.') > 1:
                liblink = liblink[:liblink.rindex('.')]
                
                os.symlink(os.path.join(dest_dir, libname),
                           os.path.join(dest_dir, liblink))
    
    def _prepare_test_dir(self, test):
        self.copy_log = ''
        
        os.mkdir(self.test_dir)        
        
        if sys.platform != 'win32':
            os.mkdir(self.lib_dir)
            os.mkdir(self.bin_dir)
        
        if not test.is_dist:
            self.copy_test_files(self.lib_dir, test.libs)  
            self.setup_versioned_libs(self.lib_dir, test.libs)
        
            self.copy_test_files(self.bin_dir, test.bins)
            
            os.mkdir(self.mod_dir)
            if test.mods:
                self.copy_test_files(self.mod_dir, test.mods)
        
        self.copy_test_files(self.test_dir, test.uses, log_copy=True)
        
    
    def prepare_test_dir(self, test):
        ''' Creates necessary directories in test directory 
            and copies files to them'''
        
        if sys.platform == "win32":
            return self._prepare_test_dir(test)
        
        # This is totally crazy. When we are in multi-threaded mode, 
        # subprocess.Popen() will fork(), and by doing that, it will 
        # clone file-open-table, including binaries that are copying
        # by other threads. There is a possibility that forked process
        # won't close binary when copying will finish and another
        # fork will start a process, which will eventually cause 
        # ETXTBSY on Linux. 
        #
        #      Runner-1                 Runner-2
        #   open(dest_bin_file)     subprocess.Popen()
        #                                   os.fork()
        #                       
        # %   At this point, two processes are opened dest_bin_file,
        # %  one is run-suite.py (in Runner-1), another is forked process
        # %  by Runner-2.
        #
        #   close(dest_bin_file)
        #
        #   subprocess.Popen()
        #        os.fork()
        #        os.execve(dest_bin_file) 
        #                  ---> ETXTBSY
        #
        # %   Runner-2 forked process still holds dest_bin_file
        #
        #                                   os.closerange(0, 256)
        #
        # %                          Too late, pal!
        #
        # To prevent this, we will do all the copying in a forked process, 
        # which will die and closes all copied files completely.
        
        # Flush any data in standard streams so it won't be copied into forked process
        sys.stdout.flush()
        sys.stderr.flush()
        
        rpipe, wpipe = os.pipe()
        
        pid = os.fork()
        if pid == -1:
            raise TestError("Failed to fork() in prepare_test_dir")
        
        if pid == 0:
            # Child -- do the copy, print log to pipe and exit
            try:
                os.close(rpipe)
                os.dup2(wpipe, sys.stdout.fileno())
                os.dup2(wpipe, sys.stderr.fileno())
                os.close(wpipe)
                
                self._prepare_test_dir(test)
                
                sys.stdout.write(self.copy_log)
            except:
                traceback.print_exc(20, sys.stderr)
            finally:
                sys.stdout.flush()
                sys.stderr.flush()
                os._exit(1)
        
        os.close(wpipe)
        
        _, status = os.waitpid(pid, 0)
        
        # XXX: if copy_log is larger than PIPE_BUF (4-8k), everything
        # then is going badly
        outf = os.fdopen(rpipe)
        self.copy_log = outf.read()
        
        return os.WEXITSTATUS(status)
    
    def _wipe_test_dir(self):
        def onerror(function, path, excinfo):
            exc = excinfo[1]
            
            if sys.platform == 'win32':
                is_win_access_denied = isinstance(exc, WindowsError) and exc.winerror == 5
            else:
                is_win_access_denied = False
            is_unix_access_denied = isinstance(exc, OSError) and exc.errno == errno.EACCES
            is_unix_non_empty = (isinstance(exc, OSError) and 
                                    exc.errno in [errno.ENOTEMPTY, errno.EEXIST])
            
            if is_win_access_denied or is_unix_access_denied:
                # Revert access rights on files that we chmodded earlier, 
                # otherwise rmtree will return access denied on windows
                os.chmod(path, 0o0777)
                
                return function(path)
            
            if is_unix_non_empty and function is os.rmdir:
                # Recursive retry :)
                shutil.rmtree(path, onerror=onerror)                
                
        if os.path.isdir(self.test_dir):
            shutil.rmtree(self.test_dir, onerror=onerror)            
    
    def wipe_test_dir(self):
        try:
            self._wipe_test_dir()
        finally:
            if os.path.isdir(self.test_dir):
                self.suite.report_test_dir(os.listdir(self.test_dir))
        
    
    def check_core(self, cfg_name, test_group):
        ''' Checks if process dumped core. If it does,
            saves it and returns True'''
        test_name = self.suite.get_test_name(cfg_name)
        core_path = os.path.join(self.test_dir, 'core')
        
        if os.path.exists(core_path):
            core_name = 'core_' + test_name.replace('/', '_')
            
            if sys.platform == 'win32':
                core_name += '.dmp'
                dest_dir = os.path.join('build', 'test', test_group)
            else:
                dest_dir = os.path.join('build', 'test')
            
            shutil.copy(core_path, os.path.join(dest_dir, core_name))
            return True
        
        return False
    
    def generate_output(self, command, start, end, proc, core, stdout, stderr):
        outf = StringIO()
        formatter = DefaultFormatter(outf)
        
        formatter.report_param('Working directory', self.test_dir)
        formatter.report_param('Command', command)
        formatter.report_param('Time', (end - start))
        
        if self.copy_log:
            formatter.report_block('Copy log', self.copy_log)
        
        if proc.timed_out:
            formatter.report_line('Timed out')
        if core:
            formatter.report_line('Dumped core')
        
        if stdout:            
            formatter.report_block('stdout', stdout)
        if stderr:            
            formatter.report_block('stderr', stderr)
        
        log_path = os.path.join(self.test_dir, self.log_name)        
        if os.path.exists(log_path):
            log_data = file(log_path).read()
            formatter.report_block(self.log_name, log_data)
        
        return outf.getvalue()
    
    def generate_env(self, base_env={}):
        test_env = base_env.copy()
        
        if sys.platform in ['linux2', 'sunos5']:
            test_env['LD_LIBRARY_PATH'] = self.lib_dir
        
        # For all tests
        test_env['TS_HIMODPATH'] = self.lib_dir
        
        # For system tests (tsexperiment)
        test_env['TS_MODPATH'] = self.mod_dir
        test_env['TS_LOGFILE'] = self.log_name
        
        # For system tests (tsgenmodsrc)
        test_env['TSLOADPATH'] = self.test_dir
        test_env['TS_INSTALL_SHARE'] = 'share'
        test_env['TS_INSTALL_INCLUDE'] = 'include'
        test_env['TS_INSTALL_LIB'] = self.lib_subdir
        test_env['TS_INSTALL_MOD_LOAD'] = 'mod'
        
        return test_env 
    
    def generate_command(self, test, abs_bin_path=True):
        test_bin = os.path.basename(test.bins[0])
        
        if abs_bin_path:
            command = test_bin
            if not test.is_dist:
                command = os.path.join(self.bin_dir, test_bin)
        else:
            command = os.path.join(self.bin_dir.replace(self.test_dir, '.'), test_bin)
            
        if test.args is not None:
            command = '%s %s' % (command, test.args)
            
        return command
    
    def manual_run_test(self, cfg_name):
        test = self.read_test_config(cfg_name)
        
        self.prepare_test_dir(test)
        print 'Prepared test directory %s' % (self.test_dir)
        
        test_env = self.generate_env(OrderedDict())
        with open(os.path.join(self.test_dir, 'testrc'), 'w') as envf:
            for var, val in test_env.items():
                print >> envf, '{0}={1}'.format(var, val)
        
        # Generate command for running test
        command = self.generate_command(test, abs_bin_path=False)
        print 'Run "%s" to start test binary' % command
        
        # Run test shell
        env = os.environ.copy()
        env.update(test_env)
        env['PS1'] = '[tstest] \w> '
        
        proc = Popen(['/bin/bash'],
                     cwd = self.test_dir,
                     env = env)
        proc.wait()
        
    def run_test(self, cfg_name):
        # Prepare test environment and generate command for it
        test = self.read_test_config(cfg_name)
        command = self.generate_command(test)
        
        if test.is_dist:
            # Distribution-wide tests should work with default environment
            test_env = os.environ
        else:
            test_env = self.generate_env(os.environ)
               
        self.prepare_test_dir(test)
        
        # Start a test along with a timer that would check
        # If test wouldn't timeout
        start = time.time()
        proc = Popen(command, 
                     stdout = PIPE,
                     stderr = PIPE,
                     cwd = self.test_dir,
                     env = test_env,
                     shell = test.is_dist or test.args)
        proc.timed_out = False
        
        timer = Timer(test.maxtime, self.stop_test, args = [proc])
        timer.start()
        
        (stdout, stderr) = proc.communicate()
        timer.cancel()
        
        end = time.time()
        
        # Check test result: core, return value
        core = self.check_core(cfg_name, test.group)        
        
        result = self.analyze_result(test, proc)
        
        # Generate test report
        output = self.generate_output(command, start, end, proc, 
                                      core, stdout, stderr)
        
        
        # Done
        self.suite.report_test('%s/%s' % (test.group, test.name),
                               result, proc.returncode,
                               output)
    
    def stop_test(self, proc):
        proc.timed_out = True
        proc.kill()       
    
    def run(self):
        while True:
            cfg_name = self.suite.get()
            
            try:
                self.run_test(cfg_name)
            except Exception:
                self.suite.report_exc(cfg_name, sys.exc_info())
            finally:
                try:
                    self.wipe_test_dir()
                except:
                    self.suite.ignore_tests()
                    raise
            
            self.suite.done()

class TestSuite(object):
    def __init__(self):
        self.queue = Queue()
        
        self.ok_count = 0
        self.failed_count = 0
        self.error_count = 0
        
        self.report_lock = Lock()
        
        self.runner_count = int(os.getenv('TSTEST_RUNNER_THREADS', 1))
        self.create_test_dir()
        
        if sys.platform == "win32" and os.getenv('TSTEST_REDIRECTED_STDOUT'):
            # Use Unix newlines '\n' on Windows for output log
            import msvcrt
            msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
        
        self.formatter = DefaultFormatter(sys.stdout)
        self.formatter.write(self.formatter.header)
        
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
            self.test_dir = tempfile.mkdtemp(prefix='tstest/dir')
            self.test_dir_delete = True
        else:
            if not os.path.isdir(self.test_dir):
                raise TestError('Test directory %s is not a directory' % self.test_dir)
        
        if not os.path.exists(self.test_dir):
            os.makedirs(self.test_dir)    
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
    
    def ignore_tests(self):
        # Ignore all other tests because test dir is now polluted
        # this is not-normal condition, but saves us from eternal wait
        # in queue.join() in main thread
        while not self.queue.empty():
            self.queue.get()
            self.queue.task_done()
    
    def get_test_name(self, cfg_name):
        group_name = os.path.basename(os.path.dirname(cfg_name))
        test_name = os.path.basename(cfg_name)
        
        if test_name.endswith('.cfg'):
            test_name = test_name[:-4]
        
        return '%s/%s' % (group_name, test_name)
    
    def report_test(self, cfg_name, result, retcode, output):
        test_name = self.get_test_name(cfg_name)
        
        if result:
            self.ok_count += 1
            resstr = 'OK'
        else:
            self.failed_count += 1
            resstr = 'FAILED'
        
        with self.report_lock:
            self.formatter.report_test(test_name, resstr, retcode)
            self.formatter.write(output)
            self.formatter.flush()
            
            # Print result to terminal
            print >> sys.stderr, 'Running test %s... %s' % (test_name, resstr)
    
    def report_exc(self, cfg_name, exc_info):
        test_name = self.get_test_name(cfg_name)
        
        self.error_count += 1
        
        with self.report_lock:
            info = traceback.format_exception(*exc_info)
            
            self.formatter.report_test(test_name, 'ERROR')
            self.formatter.report_block('Traceback', ''.join(info))
            
            # Print result to terminal
            print >> sys.stderr, 'Running test %s... ERROR' % (test_name)
            sys.stderr.flush()
    
    def report_test_dir(self, file_list):
        with self.report_lock:
            self.formatter.report_block('Some files are left in test directory:', 
                                        '\n'.join(file_list))
    def run(self):
        args = sys.argv[1:]
        
        if '--manual' in args:
            test_cfg = args[args.index('--manual') + 1]
            
            try:
                self.runners[0].manual_run_test(test_cfg)
            finally:
                self.runners[0].wipe_test_dir()
            
            return
        
        for cfg_name in args:
            self.queue.put(cfg_name)
            
        self.queue.join()
        
        self.delete_test_dir()
        
        self.formatter.report_results(self.ok_count, self.failed_count, self.error_count)
        self.formatter.write(self.formatter.tailer)

suite = TestSuite()
suite.run()    

sys.exit(suite.failed_count)
