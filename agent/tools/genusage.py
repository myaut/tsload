import sys

try:
    usage_txt_file = file(sys.argv[1], 'r')
except IndexError:
    print >> sys.stderr, 'usage.txt not specified'
    sys.exit(1)
except OSError as ose:
    print >> sys.stderr, str(ose)
    sys.exit(1)
    
header = """
/* usage.c - generated automatically with genusage.py */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void usage(int ret, const char* reason, ...) {
    va_list va;
    
    if(ret != 0) {
        va_start(va, reason);
        vfprintf(stderr, reason, va);
        va_end(va);
    }
    
    fputs("""

footer = """
        "\\n"
        "Common options:\\n"
        "   -h - print help screen\\n"
        "   -v - print program version\\n", stderr);
    
    exit(ret);
}
"""

print header
for line in usage_txt_file:
    line = line.rstrip()
    line = line.replace('"', '\\"')
    line = '       "' + line + '\\n"'
    print line

print footer