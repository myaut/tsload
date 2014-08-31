#!/usr/bin/python

import sys
import re
import string

header = '''
/*
 This file is automatically generated by gentsobjapi.py
 Use SCons to re-generate it
*/

#ifndef TSOBJGEN_H_
#define TSOBJGEN_H_

#ifndef TSOBJ_GEN
#error Do not include tsobjgen.h directly, use tsobj.h
#endif
'''

tail = '''
#endif
'''

api_header = '''
struct tsobj_api {
'''

api_tail = '''
};

typedef struct tsobj_api tsobj_api_t;

LIBIMPORT tsobj_api_t* tsobj_api_impl;
'''

function_body = ''' {
    if(tsobj_api_impl == NULL) {
        %(retstmt)s%(retjson)s%(jsonfunc)s(%(jsonparams)s);
    }
    else {
        %(retstmt)stsobj_api_impl->%(implfunc)s(%(params)s);
    }
} '''

class APIFunction:
    re_func = re.compile('STATIC_INLINE\s+(.+)\s+(\w+)\s*\(([^\)]*)\)')
    re_param = re.compile('\s*(.+)\s+(\w+)')
    
    def __init__(self, linestack):
        self.rettype = None
        
        self.name = ''
        self.params = []
        self.params_str = ''
        self.line = ''
                
        self.linestack = linestack
    
    def parse(self, line):
        m = APIFunction.re_func.match(line)
        
        self.line = line.rstrip()
        if self.line.endswith(';'):
            self.line = self.line[:-1]
        
        self.rettype = m.group(1)
        self.name = m.group(2)
        self.params_str = m.group(3)        
        
        if(self.params_str != 'void'):
            params = self.params_str.split(',')
            
            for param in params:
                m = APIFunction.re_param.match(param)
                self.params.append(m.groups())
    
    def get_api_function(self):
        prefix = '\n'.join(s 
                           for s 
                           in self.linestack
                           if all(c in string.whitespace 
                                  for c in 
                                  s))
        
        api_name = self.name + '_impl'
        
        return prefix + '\t %s (*%s)(%s);' % (self.rettype, api_name, self.params_str)
    
    def get_function_body(self):
        retstmt = 'return ' if self.rettype != 'void' else ''        
        retjson = '(tsobj_node_t*) ' if self.rettype == 'tsobj_node_t*' else ''
        
        jsonparams = []
        params = []
        
        for ptype, pname in self.params:
            if ptype == 'tsobj_node_t*':
                jsonparams.append('(json_node_t*) ' + pname)
            else:
                jsonparams.append(pname)
            params.append(pname)
            
        jsonparams = ', '.join(jsonparams)    
        params = ', '.join(params)
        
        jsonfunc = self.name.replace('tsobj', 'json')
        implfunc = self.name + '_impl'
        
        return function_body % locals()
    

infile = file(sys.argv[1])

print header

api_functions = []

# For keeping comment lines, etc.
linestack = []

for line in infile:
    if line.startswith('STATIC_INLINE'):
        func = APIFunction(linestack)
        func.parse(line)        
        linestack = []
        
        api_functions.append(func)
    else:
        linestack.append(line)

print api_header

for func in api_functions:
    print func.get_api_function()

print api_tail

for func in api_functions:
    print ''.join(func.linestack) + func.line + func.get_function_body()

print tail        