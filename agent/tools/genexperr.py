import sys
import re

base = ''' 
/*
 This file is automatically generated by genexperr.py
 Use SCons to re-generate it
*/
#include <commands.h>
#include <experiment.h>

#include <stdio.h>

const char* experr_well_known[] = {
    %(wkes)s
};

const char* experr_stages[] = {
    %(stages)s
};

static void experr_print_impl(const char* func, const char* name, int code) {
    int err_code = code & 0xff;
    int err_stage = (code >> 8) & 0xf;

    const char* wke = (err_code < %(wke_count)d) 
            ? experr_well_known[err_code] : ""; 
    const char* stage = (code != 0 && err_stage < %(stage_count)d) 
            ? experr_stages[err_stage] : "";
        
    printf("0x%%-5x %%-%(max_name_len)ds %%7d %%3d %%-%(max_func_len)ds %%-%(max_stage_len)ds %%-%(max_wke_len)ds\\n", 
            code, name, code, -tse_experr_to_cmderr(code), func, stage, wke);
}

#define experr_print(func, code)    experr_print_impl(func, #code, code)

int tse_exp_err(experiment_t* root, int argc, char* argv[]) {
    printf("%%7s %%-%(max_name_len)ds %%7s %%3s %%-%(max_func_len)ds %%-%(max_stage_len)ds  %%-%(max_wke_len)ds\\n", 
            "HEXCODE", "EXPERR",  "CODE", "RET", "FUNC", "STAGE", "WELL-KNOWN-ERROR");
    %(errors)s
    
    return CMD_OK;
}
'''

def constants_to_array(constants):
    max_len = max(map(len, constants.values())) + 1
    count = max(constants.keys()) + 1
    
    const_arr = []
    
    for i in range(count):
        const_arr.append('"%s"' % constants.get(i, '?'))
    
    return max_len, count, ',\n    '.join(const_arr)

well_known_errors = {0: 'OK'}
stages = {}
errors = []

MODE_NONE = 0
MODE_PARSE = 1
MODE_WKE = 2
MODE_STAGE = 3
MODE_ERRORS = 4

mode = MODE_NONE

re_new_state = re.compile('//\s*\[(\w+)\]')
STATES = {'WKE': MODE_WKE, 'STAGE': MODE_STAGE, 'ERRS': MODE_ERRORS}

experiment_h = file(sys.argv[1], 'r')
function = ''

for line in experiment_h:
    line = line.strip()
    
    # Parse line
    is_comment = line.startswith('//')
    new_state = None
    is_define = line.startswith('#define')    
    
    if is_comment:
        m = re_new_state.match(line)
        if m is not None:
            new_state = m.group(1)
    
    if mode >= MODE_PARSE and not is_comment and not is_define:
        break
    
    if new_state:
        mode = STATES[new_state]
        continue
    
    if is_comment:
        if mode == MODE_ERRORS:
            function = line[3:]
        else:
            mode = MODE_PARSE
        continue
    
    if is_define and mode > MODE_PARSE:
        _, def_name, def_value = line.split(None, 2)
        if mode == MODE_WKE:
            well_known_errors[int(def_value, 16)] = def_name[7:]
        elif mode == MODE_STAGE:
            stages[int(def_value)] = def_name[7:]
        elif mode == MODE_ERRORS:
            errors.append((function, def_name))

max_wke_len, wke_count, wkes = constants_to_array(well_known_errors)
max_stage_len, stage_count, stages = constants_to_array(stages)

max_func_len = max(map(lambda e: len(e[0]), errors))
max_name_len = max(map(lambda e: len(e[1]), errors))

errors = '\n    '.join(map(lambda e: 'experr_print("%s", %s);' % e, errors))

print base % locals()