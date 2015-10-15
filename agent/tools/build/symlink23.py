import errno
import os
import SCons
import inspect

# There was a bug in SCons 2.3.0 which causes build failure once versioned 
# shared libraries symlinks were created. Apply dirty workaround for it by
# patching os.symlink

if SCons.__version__.startswith('2.3.0'):
    os_symlink = os.symlink
    
    def symlink_wrapper(*args):
        try:
            return os_symlink(*args)
        except OSError as ose:
            if ose.errno == errno.EEXIST and inspect.stack()[1][3] == 'VersionedSharedLibrary':
                pass
            else:
                raise

    os.symlink = symlink_wrapper
