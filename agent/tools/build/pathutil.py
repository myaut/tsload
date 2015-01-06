import os

# Define pretty synonyms for os/os.path to be conformant with SCons naming style

PathJoin = os.path.join

PathBaseName = os.path.basename
PathDirName = os.path.dirname

PathExists = os.path.exists
PathIsAbs = os.path.isabs
PathIsFile = os.path.isfile

PathAbsolute = os.path.abspath 
PathAccess = os.access
PathRelative = os.path.relpath

PathWalk = os.walk

def PathRemove(abspath, path):
    ''' This is the same as path_remove() from pathutil.c '''
    parts_abspath = abspath.rstrip(os.sep).split(os.sep)
    parts_path = path.rstrip(os.sep).split(os.sep)
    
    abspath_tail = parts_abspath[-len(parts_path):]
    
    if abspath_tail == parts_path:
        return os.sep.join(parts_abspath[:-len(parts_path)])
    
    raise ValueError("Path '%s' is not a part of a absolute path '%s'" % (path, abspath))

def PathRemoveAbs(abspath, absdir):
    ''' This is the opposite to PathRemove '''
    parts_abspath = abspath.rstrip(os.sep).split(os.sep)
    parts_absdir = absdir.rstrip(os.sep).split(os.sep)
    
    abspath_tail = parts_abspath[:len(parts_absdir)]
    
    if abspath_tail == parts_absdir:
        return os.sep.join(parts_abspath[len(parts_absdir):])
    
    raise ValueError("Path '%s' is not a part of a absolute path '%s'" % (absdir, abspath))