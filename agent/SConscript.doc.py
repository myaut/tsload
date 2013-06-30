from pathutil import *

Import('env')

tgtdir = PathJoin('#build', 'doc')
target = PathJoin(tgtdir, 'doc')

doxy_vars = {'PROJECT_NAME': env['TSPROJECT'],
             'PROJECT_NUMBER': env['TSVERSION'],
             'OUTPUT_DIRECTORY': PathJoin('build', 'doc'),
             'FILE_PATTERNS': '*.c *.h',
             'OPTIMIZE_OUTPUT_FOR_C': 'YES',
             'RECURSIVE': 'NO',
             'EXTRACT_ALL': 'YES',
             'GENERATE_XML': 'NO',
             'GENERATE_HTML': 'YES',
             'GENERATE_RTF': 'NO',
             'GENERATE_LATEX': 'NO'}

def GenerateDoxyConfig(target, source, env):
    config_path = str(target[0])
    config_file = open(config_path, 'w')
    
    for key, value in doxy_vars.iteritems():
        config_file.write('%s = %s\n' % (key, value))
    
    input = ' '.join(map(str, source))
    config_file.write('INPUT = %s\n' % input)
    
    config_file.close()

DoxyConfigBuilder = Builder(action=GenerateDoxyConfig,
                            suffix='.doxy')

env.Append(BUILDERS = {'DoxyConfigBuilder': DoxyConfigBuilder})