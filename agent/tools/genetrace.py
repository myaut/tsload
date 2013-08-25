#!/usr/bin/python

usage = '''Genetrates ETW manifest or USDT probes from source file
 Usage: genetrace.py -m USDT|ETW source.c > manifest.man
 Environment variables:
   ETWDEBUG     - enables debug mode
   ETWEXEFILE   = path to executable
 
 To import manifest, issue:
     wevtutil im manifest.man'''

import os 
import sys
import getopt

import re
import uuid

MODE_ETW = 0
MODE_USDT = 1

opts, args = getopt.getopt(sys.argv[1:], 'm:')

opts = dict(opts)
try:
    mode = {'ETW': MODE_ETW,
            'USDT': MODE_USDT}[opts['-m']]
except KeyError as ke:
    print >> sys.stderr, 'Missing -m option, or invalid option argument (%s) ' % str(ke)
    print >> sys.stderr, usage
    sys.exit(1)

def convert_name(name, provider = False):
    '''Converts ETRC naming conventions to ETW:
        tsload__provider => Tsload-Provider
        tsload__event => TsloadEvent
    or USDT: tsload__event => tsload-event'''
    if mode == MODE_ETW:
        delim = '-' if provider else ''
        
        return delim.join([w.title()
                        for w 
                        in name.split('_') 
                        if w])
    elif mode == MODE_USDT:
        return name

class Provider:
    def __init__(self, name, guid):
        self.etrc_name = name
        self.name = convert_name(name, True)
        self.guid = guid
        
        self.events = []
        self.eventmap = {}
        
    def add_event(self, event):
        self.events.append(event)
        self.eventmap[event.etrc_name] = event
    
    def get_event(self, event_name):
        return self.eventmap[event_name]
        
class Event:
    def __init__(self, name, id):
        self.etrc_name = name
        self.name = convert_name(name)
        self.id = id
        
        self.args = []
    
    def add_arg(self, arg_type, arg_name):
        self.args.append((arg_type, arg_name))

# ----------------------
# Manifest generators

etw_type_map = {    
                'char':      'win:Int8',
                'int8_t':    'win:Int8', 
                'uint8_t':   'win:UInt8',
                'int16_t':   'win:Int16', 
                'uint16_t':  'win:UInt16',
                'int32_t':   'win:Int32', 
                'uint32_t':  'win:UInt32',
                'int64_t':   'win:Int64', 
                'uint64_t':  'win:UInt64',
                'float':     'win:Float', 
                'double':    'win:Double',
                'boolean_t': 'win:Boolean'
               }

def ctype_to_etwtype(arg_type):
    if arg_type == 'char*':
        return 'win:AnsiString'
    elif arg_type.endswith('*'):
        return 'win:Pointer'
    elif arg_type in etw_type_map:
        return etw_type_map[arg_type]
    else:
        return 'win:Binary'

def generate_etw(providers):
    from xml.etree import ElementTree as etree
    
    etree.register_namespace('xml:xs', 'http://www.w3.org/2001/XMLSchema')
    
    root = etree.Element('instrumentationManifest',
                          { 'xmlns': "http://schemas.microsoft.com/win/2004/08/events",
                            'xmlns:win': "http://manifests.microsoft.com/win/2004/08/windows/events",
                            'xmlns:xs' : "http://www.w3.org/2001/XMLSchema"  })
    man = etree.ElementTree(root)
    
    instrumentation = etree.Element('instrumentation')
    localization = etree.Element('localization')
    
    loc_resources = etree.Element('resources', { 'culture': 'en-US' } )
    loc_string_table = etree.Element('stringTable')
    
    localization.append(loc_resources)
    loc_resources.append(loc_string_table)
    
    root.extend((instrumentation, localization))
    
    instrumentation_events = etree.Element('events')
    instrumentation.append(instrumentation_events)
    
    for provider in providers.values():
        provider_msgid = iter(xrange(1, sys.maxint))
        
        provider_root = etree.Element('provider',
                                      { 'name': provider.name,
                                        'guid': '{%s}' % str(provider.guid).upper(),
                                        'symbol': provider.etrc_name,
                                        'resourceFileName': exe_fn,
                                        'messageFileName': exe_fn})
        
        debug_channel_name = '%s/Debug' % provider.name
        debug_channel = etree.Element('channel',
                                      { 'name': debug_channel_name,
                                        'chid': debug_channel_name,
                                        'type': 'Debug', 
                                        'enabled': 'false'})        
        
        prov_events = etree.Element('events')
        prov_channels = etree.Element('channels')
        prov_opcodes = etree.Element('opcodes')
        prov_templates = etree.Element('templates')
        
        prov_channels.append(debug_channel)
        
        provider_root.extend((prov_events, prov_channels, prov_opcodes, prov_templates))
        
        for event in provider.events:
            event_template_name = 'T' + event.name
            
            event_msg_name = '%s.event.%d.message' % (provider.name, next(provider_msgid))
            event_msg_args = ', '.join('%' + str(i) 
                                       for i 
                                       in range(1, len(event.args) + 1))
            event_message = '%s(%s)' % (event.name, event_msg_args)
            
            event_tag = etree.Element('event',
                                      { 'symbol': event.name,
                                        'value': str(event.id),
                                        'version': str(1),
                                        'channel': debug_channel_name,
                                        'template': event_template_name,
                                        'message': '$(string.%s)' % event_msg_name})
           
            opcode_tag = etree.Element('opcode',
                                       { 'name': event.name,
                                         'symbol': event.etrc_name,
                                         'value': str(event.id + 10) } )
            prov_opcodes.append(opcode_tag)
            
            message_tag = etree.Element('string',
                                        { 'id': event_msg_name,
                                          'value': event_message })
                                          
            template_root = etree.Element('template', { 'tid': event_template_name })
            
            for arg_type, arg_name in event.args:
                data_tag = etree.Element('data',
                                         { 'name' : arg_name,
                                           'inType': ctype_to_etwtype(arg_type)})
                template_root.append(data_tag)
            
            prov_templates.append(template_root)
            prov_events.append(event_tag)
            loc_string_table.append(message_tag)
        
        instrumentation_events.append(provider_root)
        
        
    man.write(sys.stdout)


def ctype_to_dtrace_type(arg_type):
    return 'uintptr_t'

def generate_usdt(providers):
    for provider in providers.values():
        print 'provider %s' % provider.name
        print '{'
        
        for event in provider.events:
            arg_types_list = map(ctype_to_dtrace_type, 
                                 [arg[0] for arg in event.args])
            arg_types_str = ', '.join(arg_types_list)
            
            comment = ', '.join(arg[0] + ' ' + arg[1] 
                                for arg 
                                in event.args)
            
            probe = '\tprobe %s(%s);' % (event.name, arg_types_str)
            
            if comment:
                probe += '\t/*%s*/' % comment
            
            print probe
        
        print '};'
        
# ----------------------        
# Parser

debug = False
if 'ETRACEDEBUG' in os.environ:
    debug = True

try:
    source_fn = args.pop(0)
    exe_fn = args.pop(0)
except (IndexError, KeyError):
    print >> sys.stderr, usage
    sys.exit(1)

source_file = file(source_fn, 'r')

source = source_file.read()

pos = 0
brace_pos = 0
end_pos = 0

guid_re = re.compile('0x([0-9a-fA-F]*)')

providers = {}
prov_guids = {}

while True:
    try:
        pos = source.index('ETRC', pos)
    except ValueError:
        break
    
    if source[pos:pos+9] == 'ETRC_GUID':
        # Parse GUID
        # Windows defines GUID as DWORD, WORD, WORD, and array of 8 bytes, while 
        # UUID constructor accepts last 8 bytes as huge integer
        # Every field considered to be a hexademical C literal
        
        end_pos = source.index('\n', pos)
        
        guid_name, guid = source[pos:end_pos].split(None, 1)
        
        guid_name = guid_name.strip()
        guid = guid.strip()
        
        guid_fields = map(lambda w: int(w, 16),
                          filter(lambda w: w, guid_re.findall(guid)))
        guid_last_field = reduce(lambda x, y: (x << 8 | y), guid_fields[5:])              
        guid_fields = guid_fields[:5] + [guid_last_field]
        
        guid = uuid.UUID(fields = guid_fields)
        
        prov_guids[guid_name] = guid
        
        if debug:
            print >> sys.stderr, '\tGUID', repr(guid_name), guid
        
        pos = end_pos
        continue
    
    end_pos = source.index(';', pos)
    
    expr = source[pos:end_pos]
    
    lbrace = expr.index('(')
    rbrace = expr.rindex(')')
    
    params = expr[lbrace+1:rbrace]
    
    if expr.startswith('ETRC_DEFINE_PROVIDER'):
        # Parse provider specification
        name, guid_name = params.split(',', 1)
        
        name = name.strip()
        
        guid_name = guid_name.strip()
        guid = prov_guids[guid_name]
        
        provider = Provider(name, guid)
        providers[name] = provider
        
        if debug:
            print >> sys.stderr, '\tPROVIDER', provider.name, guid
    elif expr.startswith('ETRC_DEFINE_EVENT'):
        provider_name, event_name, event_id = params.split(',', 2)
        
        provider_name = provider_name.strip()
        event_name = event_name.strip()
        event_id = int(event_id.strip())
        
        event = Event(event_name, event_id)
        
        try:
            provider = providers[provider_name]
        except KeyError:
            print >> sys.stderr, 'Provider %s is not found' % provider_name
            sys.exit(1)
            
        provider.add_event(event)
            
        if debug:
            print >> sys.stderr, '\tEVENT', provider.name, event.name, event_id
    elif expr.startswith('ETRC_PROBE'):
        # Assuming maximum number of probe arguments is 9
        
        args_count = int(expr[lbrace - 1])
        params_list = params.split(',', 2 + 2 * args_count)
        
        provider_name = params_list.pop(0).strip()
        event_name = params_list.pop(0).strip()
        
        args = zip(params_list[::2], params_list[1::2])
        
        try:
            provider = providers[provider_name]
            event = provider.get_event(event_name)
        except KeyError:
            print >> sys.stderr, "Provider %s or it's event is not found" % (provider_name, event_name)
            sys.exit(1)
            
        for arg_type, arg_name in args:
            if debug:
                print >> sys.stderr, '\tEVENT ARG', arg_type, arg_name
            
            event.add_arg(arg_type.strip(), arg_name.strip())
    
    pos = end_pos

# ----------------------
# Dump manifest to stdout

if mode == MODE_USDT:
    generate_usdt(providers)
elif mode == MODE_ETW:
    generate_etw(providers)