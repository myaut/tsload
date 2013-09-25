'''
Created on Jul 9, 2013

@author: myaut
'''
 
def sizeToHuman(size):
    '''size to human representation
        See http://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
    '''
    for x in ['b','KB','MB','GB']:
        if size < 1024.0:
            if x == 'b':
                return '%d%s' % (int(size), x)
            
            return "%3.1f%s" % (size, x)
        size /= 1024.0
    return "%3.1f%s" % (size, 'TB')

def tstimeToHuman(time):
    '''ts time (represented in nanoseconds) to human'''
    for x in ['ns','us','ms']:
        if time < 1000.0:
            if x == 'ns':
                return '%d%s' % (int(time), x)
            
            return "%3.1f%s" % (time, x)
        time /= 1000.0
    return "%3.1f%s" % (time, 's')

def _parseParam(s, suffixes):
    k = 1
    s = s.lower()
    
    for suffix, k in suffixes:
        if s.endswith(suffix):
            # Cut suffix
            s = s[:-len(suffix)]
            break
    
    if '.' in s:
        val = float(s)
    else:
        val = int(s)
        
    return int(val * k)

_sizeSuffixes = [('b', 1),
                 ('k', 1024),      ('kb', 1024),
                 ('m', 1024 ** 2), ('mb', 1024 ** 2),
                 ('g', 1024 ** 3), ('gb', 1024 ** 3)]
def parseSizeParam(s):
    return _parseParam(s, _sizeSuffixes)

_timeSuffixes = [('d',   86400 * (1000 ** 3)),
                 ('h',   3600 * (1000 ** 3)),
                 ('min', 60 * (1000 ** 3)),
                 ('s',   1 * (1000 ** 3)),
                 ('ms',  1 * (1000 ** 2)),
                 ('us',  1 * 1000),
                 ('ns',  1)]
def parseTimeParam(s):
    return _parseParam(s, _timeSuffixes)