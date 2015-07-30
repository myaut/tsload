#!/usr/bin/python

import os
import sys

import subprocess

# debchangelog.py
# Generates debian/changelog file based on commit history
#
# Usage: debchangelog.py distro /path/to/tsload/agent
#
# Uses pkg/deb-versions file to generate it
#

def read_deb_versions(repo):
    ''' Reads pkg/deb-versions and return list in following form:
        [version, commit-start, commit-end]'''
        
    version = None   
    prev_commit = None
    
    versions = []
    
    with open(os.path.join(repo, 'pkg', 'debian', 'changelog.in')) as f:
        for line in f:
            if line[0] == '#':
                continue
            line = line.strip()
            
            vr, name, commit = line.split()
            if vr == 'v':
                version = name
                prev_commit = commit
            elif vr == 'r':
                if version is None:
                    raise ValueError('Release goes before version')
                
                versions.append(('{0}~{1}'.format(version, name),
                                 prev_commit, commit))
                prev_commit = commit
            else:
                raise ValueError("Invalid deb-versions line: " + line)
     
    return versions

def group_git_log_output(out):
    # Split git log output into lines and yield in pieces (one piece = one commit)
    
    lines = out.split('\n')
    commit_lines = []
    
    for line in lines:
        line = line.strip()
        
        if line.startswith('commit'):
            if commit_lines:
                yield commit_lines
            commit_lines = []
        commit_lines.append(line)
    
    yield commit_lines
    
def date_git2changelog(date):
    l = date.split()
    # Mon  Jul 20 22:44:24 2015 +0300
    # Mon, 20 Jul 2015 22:44:24 +0300
    
    l2 = [l[0] + ',', l[2], l[1], l[4], l[3], l[5]]
    return ' '.join(l2)

def generate_change(repo, distro, version, commit_start, commit_end):
    # Calls git log, parses its output and prints to stdout
    git = subprocess.Popen("git log --abbrev-commit {0}..{1}".format(commit_start, commit_end),
                           shell=True,
                           cwd=repo,
                           stdout=subprocess.PIPE)
    out, _ = git.communicate()
    
    author = None
    date = None
    commits = []    # In format (abbr-hash, message)
    
    for commit in group_git_log_output(out):
        abbrev = commit[0].split()[1]
        author = commit[1].split(None, 1)[1]
        date = commit[2].split(None, 1)[1]
        message = commit[4]
        
        commits.append((abbrev, message))
    
    if commits:
        # Finally, print to stdout
        # XXX: Prints author of the last commit!
        print 'tsload ({0}) {1}; urgency=low\n'.format(version, distro)
        for abbrev, message in commits:
            print '  * [{0}] {1}'.format(abbrev, message)
        
        date = date_git2changelog(date)
        
        print 
        print ' -- {0}  {1}'.format(author, date)
        print 


distro = sys.argv[1]
repo = sys.argv[2]
versions = read_deb_versions(repo)

for ver in reversed(versions):
    generate_change(repo, distro, *ver)

print 
