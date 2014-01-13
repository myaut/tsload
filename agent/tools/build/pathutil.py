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