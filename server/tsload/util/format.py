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