import os
import sys

from plat import PlatFunction, PlatCache

debug = False

if 'PLATDEBUG' in os.environ:
    debug = True

if len(sys.argv) == 1 or not 'PLATCACHE' in os.environ:
    print >> sys.stderr, 'Usage: PLATCACHE="file" parse-include.py <include-file>'
    sys.exit(1)

for include_fn in sys.argv[1:]:
    include_file = file(include_fn, 'r')
    
    plat_funcs = {}
    
    PlatCache.delete(include_fn)
    
    if debug:
        print >> sys.stderr, 'Parsing %s' % str(include_fn)
    
    include = include_file.read()
    func_list = PlatFunction.re.findall(include)
    
    for ret_type, name, args in func_list:
        func = PlatFunction(name, ret_type, include_fn)
        
        plat_funcs[name] = func
        
        if debug:
            print >> sys.stderr, '\tFUNC %s' % func.name
    
    PlatCache.update(plat_funcs)
