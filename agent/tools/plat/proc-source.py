import os
import sys

from plat import PlatFunction, PlatDeclaration, PlatCache

class SourcePrinter:
    line = 0
    
    @classmethod
    def do_account(cls, s):
        cls.line += sum(1 for c in s if c == '\n')
    
    @classmethod
    def do_print(cls, s):
        # Count line breaks
        cls.do_account(s)
        
        sys.stdout.write(s)

debug = False

if 'PLATDEBUG' in os.environ:
    debug = True

if len(sys.argv) == 1 or not 'PLATCACHE' in os.environ:
    print >> sys.stderr, 'Usage: PLATCACHE="file" process-source.py <source-file>'
    sys.exit(1)

plat_funcs = {}
try:
    plat_funcs = PlatCache.read()
except Exception as e:
    print >> sys.stderr, "Couldn't read PLATAPI cache file %s: %s" % (os.environ['PLATCACHE'], str(e))
    sys.exit(1)

source_fn = sys.argv[1]
source_file = file(source_fn, 'r')

if debug:
    print >> sys.stderr, 'Processing %s' % source_fn

source = source_file.read()
old_pos, pos = 0, 0
func_pos = 0
brace_pos = 0
decl_end_pos = 0
braces = 1

remove = False

while True:
    # Find next PLATAPI function
    old_pos = pos
    try:
        func_pos = pos = source.index('PLATAPI', pos)
        
        # Write information data PLATAPI functions
        SourcePrinter.do_print(source[old_pos:pos])
    except ValueError:
        # PLATAPI function was not found - Print last piece of file
        SourcePrinter.do_print(source[old_pos:len(source)])
        break 
    
    # Process platform-dependent declarations
    if source[pos:].startswith('PLATAPIDECL'):
        decl_end_pos = source.index(';', pos)
        
        decl_str = source[pos:decl_end_pos+1]
        decl = PlatDeclaration.re.match(decl_str)
        
        if decl is None:
            print >> sys.stderr, '%s:%d > Invalid PLATAPIDECL "%s" found' % (source_fn, SourcePrinter.line, decl_str)
            sys.exit(1)
        
        func_names = [fname.strip() for fname in decl.group(1).split(',')]
        
        # If any of function needs this declaration, leave it
        if any(func.may_impl(source_fn) 
               for func in plat_funcs.values() 
               if func.name in func_names):
                   SourcePrinter.do_print(decl_str)
        
        # Continue
        pos = decl_end_pos + 1
        continue
    
    # Find entrance to function's body
    brace_pos = source.index('{', pos)
    
    func_list = PlatFunction.re.findall(source[pos:brace_pos])
    
    if len(func_list) != 1:
        print >> sys.stderr, '%s:%d > Invalid PLATAPI function in declaration' % (source_fn, SourcePrinter.line)
        sys.exit(1)
    
    func_name = func_list[0][1]
    func_ret_value = func_list[0][0]
    
    # Try to find function in plat_funs and set implementation of it
    remove = False
    try:
        plat_func = plat_funcs[func_name]
        
        if not plat_func.set_impl(source_fn):
            # Function already implemented - remove implementation from source
            if debug:
                print >> sys.stderr, '%s:%d > REMOVE %s FROM %s IMPL %s' % (source_fn, 
                                            SourcePrinter.line, func_name, source_fn, plat_func.impl)
            remove = True
        else:
            # Say platcache that we are implementing this function
            PlatCache.update_func(plat_func)
    except KeyError:
        print >> sys.stderr, '%s:%d > %s is not platform-dependent function' % (source_fn, SourcePrinter.line, func_name)
        sys.exit(1)
    
    # Scan source for ending brace
    # If there are blocks in function body, than 
    # they will be counted in braces counter
    brace_pos += 1
    braces = 1
    while brace_pos < len(source):        
        if source[brace_pos] == '{':
            braces += 1
        elif source[brace_pos] == '}':
            braces -= 1
            
        if braces == 0:
            break
        
        brace_pos += 1
    else:
        print >> sys.stderr, '%s:%d > Unfinished braces at function %s!' % (source_fn, SourcePrinter.line, func_name)
        sys.exit(1)
    
    # Found ending of function body, print it, if
    pos = brace_pos + 1
    if not remove:
        SourcePrinter.do_print(source[func_pos:pos])
    else:
        SourcePrinter.do_account(source[func_pos:pos])