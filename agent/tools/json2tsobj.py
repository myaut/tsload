import re
import sys

'''JSON to tsobj - converts old-style json_push_back formatters to new-style tsobj_add'''

# An example is json_push_back(jrq, json_new_a("workload_name", rq->rq_workload->wl_name));

re_push_back = re.compile('\s*json_push_back\(\s*(\w+)\s*,\s*json_new_([aibf])\(\s*"(\w+)"\s*,\s*(.*)\s*\)\)\s*;\s*')
tsobj_add = 'tsobj_add_%(typetag)s(%(node)s, TSOBJ_STR("%(field)s"), %(value)s);'

lines = []
for line in sys.stdin:
    lines.append(line)

types = {'a': 'string',
         'b': 'boolean',
         'f': 'double',
         'i': 'integer'
         }

for line in lines:
    m = re_push_back.match(line)
    
    if m:
        node, typetag, field, value = m.groups()
        typetag = types[typetag]
        
        if typetag == 'string':
            if value[0] == '"':
                value = 'TSOBJ_STR(%s)' % value
            else:
                value = 'tsobj_str_create(%s)' % value
        
        print tsobj_add % locals()
    else:
        sys.stdout.write(line)