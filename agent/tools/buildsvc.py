import os
import sys

import getopt
import string

from fnmatch import fnmatch
from ConfigParser import ConfigParser

import paramiko as sshclient
import time

# Shared with SConscript.install.py
sys.path.append(os.path.join(os.path.dirname(__file__), 'build'))
from installdirs import install_dirs

class Repository(object):
    '''Represents local source repository of TSLoad Agent. Walks over
    sources, saving their names and os.stat data. Ignores files that 
    match .gitignore files. Iterable (provides list of Repository.File
    objects) 
    
    Public members:
        - repo_path - absolute path to TSLoad source repository
    '''
    class File(object):
        def __init__(self, path, abspath):
            self.path = path
            self.stat = os.stat(abspath)
        
        def newer(self, stat):
            '''Checks if local file is newer than remote file
            identified by stat structure'''
            # Truncate to int because paramiko provides integers 
            # in stat structure. Otherwise, file would be always 
            # newer than remote one 
            return int(self.stat.st_mtime) > int(stat.st_mtime)
        
        def get_times(self):
            '''Returns pair (atime, mtime) for SFTPClient.utime() call'''
            return (self.stat.st_atime, self.stat.st_mtime)
    
    def __init__(self, repo_path):
        self.repo_path = repo_path
        self.ignore = ['.git']
        
        self.filelist = []
        
        self.basedir = os.getenv('TSBUILD_BASE_DIR', '')
        self._walk(self.basedir)
        
    def __iter__(self):
        return iter(self.filelist)
    
    def _walk(self, subpath):
        ignore_orig = self.ignore[:]
        
        abspath = os.path.join(self.repo_path, subpath)
        self._add_gitignore(abspath)
        
        for filename in os.listdir(abspath):
            if self.is_ignored(filename, subpath):
                continue
            
            fileabspath = os.path.join(abspath, filename) 
            filepath = os.path.join(subpath, filename) 
            self.filelist.append(Repository.File(filepath, fileabspath))
            
            if os.path.isdir(fileabspath):
                self._walk(filepath)
        
        self.ignore = ignore_orig
    
    def _add_gitignore(self, path):
        gitignore = os.path.join(path, '.gitignore')
        
        if os.path.exists(gitignore):
            f = file(gitignore, 'r')
            patterns = f.readlines()
            self.ignore.extend(fnpattern.strip() 
                               for fnpattern in patterns)
            f.close()
    
    def is_ignored(self, filename, subpath):
        '''Checks if file is ignored by .gitignore directives'''
        ignored = False
        relpath = os.path.join(subpath, filename)
        
        for fnpattern in self.ignore:
            if fnpattern[0] == '!':
                fnpattern = fnpattern[1:]
                value = False
            else:
                value = True
            
            if fnmatch(filename, fnpattern) or fnmatch(relpath, fnpattern):
                ignored = value
        
        return ignored

class BuildError(Exception):
    pass

class TestRunError(Exception):
    pass

class BuildServer(object):
    '''Remote BuildServer
    
    Connects using SSH/SFTP to remote build server and executes build
    actions on that'''
    
    BUF_SIZE = 4096
    
    LOG_ALL = 0
    LOG_FILE = 1
    
    BUILD_DIR = 'build'
    PKG_DIR = 'pkg'
    
    def __init__(self, name, hostname, username, repodir, plat, mach, 
                 port=22, opts='', pubkey='', password='', scons='scons'):
        self.name = name
        
        self.hostname = hostname
        self.port = int(port)
        self.repodir = repodir
        
        self.username = username
        self.pubkey = pubkey
        self.password = password
        
        self.plat = plat
        self.mach = mach
        
        self.scons = scons        
        self.opts = opts
        
        self.env = {}
        self._setup_env()
        
        self.builddir = None
        self.log_file = None
    
    def _setup_env(self):
        if self.plat == 'win32':
            path_win2posix = lambda path: path.replace('\\', '/') if path else './'
            
            subdirs = dict((d[0], path_win2posix(d[2])) 
                           for d 
                           in install_dirs)
            self.env['EXESUFFIX'] = '.exe'
        else:
            subdirs = dict((d[0], d[1]) 
                           for d 
                           in install_dirs)
            self.env['EXESUFFIX'] = ''
            
        
        self.env.update(subdirs)
    
    def connect(self):
        '''Connects to remote build server.
        
        NOTE: May raise paramiko exceptions'''
        print 'Connecting to %s:%d...' % (self.hostname, self.port)
        
        self.transport = sshclient.Transport((self.hostname, self.port))
        self.transport.connect()
        
        if self.pubkey:
            rsapub_path = os.path.expanduser('~/.ssh/id_rsa')
            pkey = sshclient.RSAKey(filename = rsapub_path)
            self.transport.auth_publickey(self.username, pkey)
        elif self.password:
            self.transport.auth_password(self.username, self.password)
        else:
            self.transport.auth_none(self.username)
        
        self.sftp = self.transport.open_sftp_client()
    
    def log_init(self, logprefix, logdir):
        '''Sets up logging to directory `logdir`'''
        self.log_path = os.path.join(logdir, logprefix + '_' + self.name + '.log')
        
    def _log(self, level, msg, *args):
        if self.log_file is None:
            self.log_file = file(self.log_path, 'w')
        
        if args:
            text = msg % args
        else:
            text = msg
        
        self.log_file.write(text)
        
        if level == BuildServer.LOG_ALL:
            sys.stdout.write(text)
            
        self.log_file.flush()
    
    def copy(self, repository):
        '''Copies updated files from repository to remote build server.
        Determines updated files by mtime modifiers. '''
        self._log(BuildServer.LOG_ALL, 'Copying files to %s:%s with base %s\n', 
                  self.hostname, self.repodir, repository.basedir)
        
        if self._remote_stat('') is None:
            self.sftp.mkdir(self.repodir)
        
        for file in repository:
            stat = self._remote_stat(file.path)
            
            if stat is None or file.newer(stat):
                localpath = os.path.join(repository.repo_path, file.path)
                remotepath = os.path.join(self.repodir, file.path)
                
                if os.path.isdir(localpath):
                    if stat is None:
                        self.sftp.mkdir(remotepath)
                    continue
                
                self._log(BuildServer.LOG_FILE, '\t%s', file.path)
                
                self.sftp.put(localpath, remotepath)
                self.sftp.utime(remotepath, file.get_times())
    
    def build(self, opts, target):
        '''Builds target on remote build server. If build
        fails, raises BuildError. 
        
        NOTE: On Windows with Cygwin you need to set scons= property
        in config file so BuildServer correctly execute scons. '''
        self._log(BuildServer.LOG_ALL, 'Building %s\n', target)
        command = '%s %s %s %s' % (self.scons, opts, self.opts, target)
        
        retcode = self._remote_call(self.repodir, command)
        
        if retcode != 0:
            raise BuildError('Error building target "%s" on %s' % (target, self.name))
    
    def get_results(self, prefix):
        filenames = self._get_results(prefix, self.plat)
        
        if len(filenames) != 1:
            raise BuildError('Could not find resulting build!')
        
        self.outputdir = os.path.join(self.repodir, BuildServer.BUILD_DIR, filenames[0])
        self._log(BuildServer.LOG_ALL, 'Output directory is %s\n', self.outputdir)
    
    def test_run(self, command):
        '''Runs test executable on remote build server.
        
        NOTE: $INSTALL_BIN and other vars should be used in executable name
        and arguments. I.e.:
            ${INSTALL_BIN}tshostinfo -x
        '''
        retcode = self._remote_call(self.outputdir, command)
        
        if retcode != 0:
            raise TestRunError('Error in test run on %s' % (self.name))
    
    def fetch(self, outdir):
        '''Fetches resulting packages from build/pkg on remote build server
        and puts them into output directory.'''
        suffix = '-%s-%s' % (self.plat, self.mach)
        pkgpath = os.path.join(self.repodir, BuildServer.BUILD_DIR, 
                               BuildServer.PKG_DIR)
        pkgs = self.sftp.listdir(pkgpath)
        
        for remotename in pkgs:            
            pkg_parts = list(remotename.rpartition('.'))
            pkg_parts.insert(1, suffix)
            localname = ''.join(pkg_parts)
            
            localpath = os.path.join(outdir, localname)
            remotepath = os.path.join(pkgpath, remotename)
            
            self._log(BuildServer.LOG_ALL, 'Fetching %s -> %s\n', remotename, localname)
            
            self.sftp.get(remotepath, localpath)            
    
    def fetch_log(self, outdir, logpath):
        logname = os.path.basename(logpath)
        
        remotepath = os.path.join(self.repodir, logpath)
        localpath = os.path.join(outdir, self.name + '_' + logname)
        
        self._log(BuildServer.LOG_ALL, 'Fetching %s -> %s\n', remotepath, localpath)
        
        self.sftp.get(remotepath, localpath)
    
    def _remote_stat(self, remote_path):
        try:
            path = os.path.join(self.repodir, remote_path)
            return self.sftp.stat(path)
        except IOError:
            return None
    
    def _remote_call(self, workdir, command):
        self._log(BuildServer.LOG_ALL, 'Running %s\n', command)
        
        template = string.Template(command)
        command = template.substitute(self.env) 
        command = 'cd %s && . ~/.profile && %s' % (workdir, command)
        
        self._log(BuildServer.LOG_FILE, '\t%s\n', command)
        
        shell = self.transport.open_session()
        shell.exec_command(command)
        
        while True:
            if shell.recv_ready():
                self._log(BuildServer.LOG_FILE, 
                          shell.recv(BuildServer.BUF_SIZE))
            elif shell.recv_stderr_ready():
                self._log(BuildServer.LOG_FILE, 
                          shell.recv_stderr(BuildServer.BUF_SIZE))
            elif shell.exit_status_ready():
                retcode = shell.recv_exit_status()
                self._log(BuildServer.LOG_ALL, '\treturn code: %d\n', retcode)
                shell.close()
                return retcode
    
    def _get_results(self, prefix, suffix):
        suffix = self.plat
        buildpath = os.path.join(self.repodir, BuildServer.BUILD_DIR)
        
        filenames = filter(lambda fn: fn.startswith(prefix) and fn.endswith(suffix), 
                           self.sftp.listdir(buildpath))
        
        return filenames

class BuildManager(object):
    def __init__(self, config_path):
        self.servers = {}
        
        self._do_config(config_path)
    
    def _do_config(self, config_path):
        self.config = ConfigParser()
        self.config.read(config_path)
        
        self.global_opts = self.config.get('__global__', 'opts')
        self.log_dir = self.config.get('__global__', 'logdir')
        self.log_prefix = self.config.get('__global__', 'logprefix')
        self.out_dir = self.config.get('__global__', 'outdir')
        self.prefix = self.config.get('__global__', 'prefix')
        
        for section in self.config.sections():
            if section == '__global__':
                continue            
            name = section
            
            options = dict(self.config.items(section))
            server = BuildServer(name, **options)
            server.log_init(self.log_prefix, self.log_dir)
            
            self.servers[name] = server
    
    def list_servers(self):
        fmtstr = '%-16s %-6s %-6s %s'
        
        print fmtstr % ('NAME', 'PLAT', 'MACH', 'URI')
        
        for name in self.servers:
            server = self.servers[name]
            uri = '%s@%s:%s' % (server.username, server.hostname, 
                                server.repodir)
            
            print fmtstr % (name, server.plat, server.mach, uri)
    
    def run(self, repository, servers = [], targets = []):
        if not servers:
            servers = self.servers.keys()
        
        if not targets:
            targets = ['copy', 'tests', 'install', 'fetch', 'run']
        
        for name in servers:
            print >> sys.stderr, '-------------%s-------------' % name
            
            server = self.servers[name]
            
            server.connect()
            
            if 'copy' in targets:
                server.copy(repository)
            
            try: 
                if 'tests' in targets:
                    try:
                        server.build(self.global_opts, 'tests')
                    finally:
                        server.fetch_log(self.out_dir, 'build/test/tests.log')
                    
                if 'install' in targets:
                    server.build(self.global_opts, 'install')
                
                if 'systests' in targets:
                    try:
                        server.build(self.global_opts, 'systests')
                    finally:
                        server.fetch_log(self.out_dir, 'build/test/tsexperiment/systests.html')
                
                if 'fetch' in targets:
                    server.build(self.global_opts, 'zip')
                    server.fetch(self.out_dir)
            except BuildError as be:
                print >> sys.stderr, str(be)
            except TestRunError as tre:
                print >> sys.stderr, str(tre)
        
# main     

usage_str="""
Build-service - tool to remotely build TSLoad packages

Usage:
$ python buildsvc.py [OPTIONS] [TARGETS]

Where options are
 * -c /path/to/tsload-build.cfg 
 * -r /path/to/tsload/agent 
 * [-n node [ -n node ...]]

To list remote build servers, specify -l option:
$ python buildsvc.py -c /path/to/tsload-build.cfg -l

Set TSBUILD_BASE_DIR to base path of copied files to make copying faster."""

def usage(error):
    print >> sys.stderr, error
    print >> sys.stderr, usage_str
    sys.exit(2)

def error(error):
    print >> sys.stderr, error
    sys.exit(1)   
    
try:
    opts, args = getopt.getopt(sys.argv[1:], 'lc:r:n:')
except getopt.GetoptError as goe:
    usage(str(goe))

nodes = []
config_path = None
repo_path = None
lflag = False
for opt, optarg in opts:
    if opt == '-c':
        config_path = optarg
    elif opt == '-r':
        repo_path = optarg
    elif opt == '-l':
        lflag = True
    elif opt == '-n':
        nodes.append(optarg)
    else:
        usage('Unknown option %s' % opt)

if config_path is None:
    usage('Missing config path')

manager = BuildManager(config_path)

if lflag:
    manager.list_servers()
    sys.exit(0)

if repo_path is None:
    usage('Missing repository path')


repository = Repository(repo_path)
manager.run(repository, nodes, args)