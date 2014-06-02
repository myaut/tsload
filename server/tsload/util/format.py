
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
