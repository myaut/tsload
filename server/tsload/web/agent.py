'''
Created on 02.06.2013

@author: myaut
'''

from nevow import livepage
from nevow import loaders
from nevow import rend
from nevow import inevow
from nevow import url
from nevow import flat
from nevow import tags as T

from twisted.internet.defer import inlineCallbacks, returnValue

from tsload.web.main import MainPage, LiveMainPage, Menu
from tsload.web.pagination import PaginatedView

from tsload.jsonts.api.user import TSUserDescriptor
from tsload.jsonts.api.load import *

from tsload.wlparam import WLParamHelper
from tsload.util.format import tstimeToHuman, sizeToHuman

osIcons = [('debian', 'debian.png'),
           ('suse', 'suse.png'),
           ('rhel', 'redhat.png'),
           ('centos', 'redhat.png'),
           ('ubuntu', 'ubuntu.png'),
           ('solaris', 'solaris.png'),
           ('windows', 'windows.png')]

wlclasses = {'cpu_integer': ('CPU', 'CPU integer operations'),
             'cpu_float': ('CPU', 'Floating point operations'),
             'cpu_memory': ('CPU', 'CPU memory/cache operations'),
             'cpu_misc': ('CPU', 'CPU workload (misc.)'),
             
             'mem_allocation': ('MEM', 'Memory allocation'),
             
             'fs_op': ('FS', 'Filesystem operations'),
             'fs_rw': ('FS', 'Filesystem read/write'),
             
             'disk_rw': ('DISK', 'Raw disk read/write'),
             
             'network': ('NET', 'Network benchmark'),
             
             'os': ('OS', 'Miscellanous os benchmark')}

def _wlparamToStr(val, wlp):
    if isinstance(wlp, TSWLParamTime):
        return tstimeToHuman(val)
    elif isinstance(wlp, TSWLParamSize):
        return sizeToHuman(val)
    
    return str(val)

class AgentMenu(Menu):
    navClass = 'nav nav-tabs'
    
    def setActiveWhat(self, what):
        self.actWhat = what
    
    def addAgentItem(self, title, what, isDisabled=False):
        self.addItem(title, 
                     url.here.add('what', what),
                     self.actWhat == what,
                     isDisabled)

class LoadAgentMenu(AgentMenu):
    navClass = 'nav nav-pills nav-stacked'

class ClientTable(PaginatedView):
    elementsPerPage = 20
    searchPlaceholder = 'Enter client type...'
    paginatedDataField = 'data_clients'
    
    def render_mainTable(self, ctx, data):
        return loaders.xmlfile('webapp/agent/client.html')
    
    def doFilter(self, criteria, row):
        return row['class'] == criteria

class LoadAgentTable(PaginatedView):
    elementsPerPage = 20
    searchPlaceholder = 'Enter hostname, osname, etc...'
    paginatedDataField = 'data_loadAgents'
    
    def render_mainTable(self, ctx, data):
        return loaders.xmlfile('webapp/agent/loadagent.html')
    
    def doFilter(self, criteria, row):
        if criteria.lower() in row['osname'].lower():
            return True
        
        return row['hostname'] == criteria

class WorkloadTypeTable(PaginatedView):
    elementsPerPage = 20
    searchPlaceholder = 'Enter workload type or module'
    paginatedDataField = 'data_workloadTypes'
    
    def render_mainTable(self, ctx, data):
        return loaders.xmlfile('webapp/agent/wltypetable.html')
    
    def doFilter(self, criteria, row):
        return row['workload'] == criteria or \
               row['module'] == criteria
    
    def render_customControls(self, ctx, data):
        handler = lambda wlc: livepage.server.handle('workloadClass', wlc)
        button = lambda wlc: T.button(_class='btn active', id='wlc_' + wlc,
                                      onclick = handler(wlc))[wlc]
        
        return T.div(_class='btn-group')[button('CPU'),
                                         button('MEM'),
                                         button('FS'),
                                         button('DISK'),
                                         button('NET'),
                                         button('OS')
                                         ]

class LoadAgentInfoPage(LiveMainPage):
    defaultView = ['agent']
    agentUuid = ''
    
    workloadTypes = None
    
    def locateChild(self, ctx, segments):
        if len(segments) == 1:
            self.agentUuid = segments[0]
            return (self, [])
        
        return LiveMainPage.locateChild(self, ctx, segments[1:])
    
    @inlineCallbacks
    def getBasicAgentInfo(self, ctx):
        session = inevow.ISession(ctx) 
        
        clientList = yield session.agent.rootAgent.listClients()
        agentList = yield session.agent.expsvcAgent.listAgents()
        
        try:
            self.agentInfo = agentList[self.agentUuid]
        except KeyError:
            self.agentInfo = None
            
        # Find client info
        
        for client in clientList:
            if client.type == 'load' and client.uuid == self.agentUuid:
                self.clientInfo = client
                self.agent = session.agent.createRemoteAgent(client.id, LoadAgent)
                break
        else:
            self.clientInfo = None
            self.agent = None
    
    @inlineCallbacks 
    def render_content(self, ctx, data):
        # FIXME: should check roles
        
        yield self.getBasicAgentInfo(ctx)
        
        if self.agentInfo is None:
            returnValue(self.renderAlert("Couldn't retrieve information for agent %s" % self.agentUuid))
        else:            
            returnValue(loaders.xmlfile('webapp/agent/loadinfo.html'))
    
    def render_agentMenu(self, ctx, data):
        menu = LoadAgentMenu()
        
        request = inevow.IRequest(ctx)
        what = request.args.get('what', self.defaultView)[0]
        menu.setActiveWhat(what)
        
        disabledInfo = self.agent is None
        
        menu.addAgentItem('Information', 'agent')
        menu.addAgentItem('Agent resources', 'resource', disabledInfo)
        menu.addAgentItem('Workload types', 'wltype', disabledInfo)
        menu.addAgentItem('Experiments', 'experiments', disabledInfo)
        
        return menu
    
    def render_agentName(self, ctx, data):
        return self.agentInfo.hostname
    
    def render_agentInfo(self, ctx, data):
        if not self.agentUuid:
            return url.here.up()
        
        request = inevow.IRequest(ctx)
        
        what = request.args.get('what', self.defaultView)
        
        if 'agent' in what:
            return self.agentCommonInfo(ctx)
        else:
            if self.agent is None:
                return self.renderAlert('Agent is disconnected')
            
            if 'wltype' in what:
                wlt = request.args.get('wlt', None)
                
                if wlt:
                    return self.workloadType(ctx, wlt[0])
                
                return self.workloadTypeList(ctx)
        
        return ''
    
    def agentCommonInfo(self, ctx):
        agentInfo = self.agentInfo
        clientInfo = self.clientInfo
        
        if clientInfo is None:
            clientStateImg = T.img(src = '/images/cl-status/dead.png')
            clientState = 'disconnected'
            
            clientId = 'N/A'
            clientUuid = 'N/A'
            clientEndpoint = 'N/A'
        else:
            clientStateImg = T.img(src = '/images/cl-status/established.png')
            clientState = 'established'
            
            clientId = clientInfo.id
            clientUuid = clientInfo.uuid
            clientEndpoint = clientInfo.endpoint
        
        for slot, data in [('hostname', agentInfo.hostname),
                            ('domainname', agentInfo.domainname),
                            ('osname', agentInfo.osname),
                            ('release', agentInfo.release),
                            ('arch', agentInfo.machineArch),
                            ('numCPUs', agentInfo.numCPUs),
                            ('numCores', agentInfo.numCores),
                            ('memTotal', agentInfo.memTotal),
                            ('agentId', agentInfo.agentId),
                            ('lastOnline', agentInfo.lastOnline),
                            ('clientStateImg', clientStateImg),
                            ('clientState', clientState),
                            ('clientId', clientId),
                            ('clientUuid', clientUuid),
                            ('clientEndpoint', clientEndpoint)]:
                ctx.fillSlots(slot, data)
            
        return loaders.xmlfile('webapp/agent/loadinfoagent.html')
    
    @inlineCallbacks
    def workloadTypeList(self, ctx):
        session = inevow.ISession(ctx) 
        wltTable = WorkloadTypeTable(self, session.uid)
        
        if self.workloadTypes is None:
            self.workloadTypes = yield self.agent.getWorkloadTypes()
        
        self.workloadClasses = ['CPU', 'MEM', 'DISK', 'FS', 'NET', 'OS']
        
        for wltypeName, wltype in self.workloadTypes.iteritems():
            wlcTags = [T.span(_class='badge', 
                              title = wlclasses[wlc][1])[
                                                         wlclasses[wlc][0]]
                       for wlc 
                       in wltype.wlclass]
            rawClasses = set(wlclasses[wlc][0] for wlc in wltype.wlclass)
            
            wltTable.add({'module': wltype.module,
                          'workload': wltypeName,
                          # url.gethere doesn't work here because when liveglue updates page
                          # all links become broken. 
                          'wltHref': url.URL.fromContext(ctx).add('wlt', wltypeName),
                          'classes': wlcTags,
                          'rawClasses': rawClasses})
        
        wltTable.setPage(0)
        self.wltTable = wltTable
        
        returnValue(wltTable)
    
    def handle_workloadClass(self, ctx, wlc):
        if wlc in self.workloadClasses:
            self.workloadClasses.remove(wlc)
            action = "remove"
        else:
            self.workloadClasses.append(wlc)
            action = "add"
            
        rawClasses = set(self.workloadClasses)
         
        yield livepage.js('document.getElementById("wlc_%s").classList.%s("active")' % (wlc, action)), livepage.eol
        
        self.wltTable.prefilteredData = [wlc 
                                         for wlc 
                                         in self.wltTable.rawData
                                         if wlc['rawClasses'] & rawClasses]
        self.wltTable.filteredData = self.wltTable.prefilteredData
        
        self.wltTable.setPage(0)
        yield self.wltTable.update(ctx)
    
    @inlineCallbacks
    def workloadType(self, ctx, wltypeName):
        if self.workloadTypes is None:
            self.workloadTypes = yield self.agent.getWorkloadTypes()
        
        wltype = self.workloadTypes[wltypeName]
        
        wlcDescriptions = [T.li[wlclasses[wlc][1]]
                           for wlc 
                           in wltype.wlclass]
        
        ctx.fillSlots('name', wltypeName)
        ctx.fillSlots('module', wltype.module)
        ctx.fillSlots('path', wltype.path)
        ctx.fillSlots('classes', T.ul()[wlcDescriptions])
        
        self.data_workloadParams = []
        
        for paramName, param in wltype.params.iteritems():
                    if param.flags & TSWLParamCommon.WLPF_OPTIONAL:
                        paramName = T.span[paramName,
                                           T.sup['OPT']]
                    
                    wlpType = WLParamHelper.getTypeName(param)
                    
                    minVal, maxVal = WLParamHelper.getIntegerRange(param)
                    lenVal = WLParamHelper.getStringLength(param)
                    range = 'None'
                    
                    if minVal is not None and maxVal is not None:
                        range = '[%s...%s]' % (_wlparamToStr(minVal, param), 
                                               _wlparamToStr(maxVal, param))
                    elif lenVal is not None:
                        range = 'len: %s' % lenVal
                    
                    default = WLParamHelper.getDefaultValue(param)
                    if default is not None:
                        default = _wlparamToStr(default, param)
                    else:
                        default = 'None'
                    
                    self.data_workloadParams.append({'param': paramName, 
                                                     'type': wlpType, 
                                                     'range': range, 
                                                     'default': default, 
                                                     'description': param.description})
        
        wltparams = loaders.xmlfile('webapp/agent/wltypeparam.html')
        
        returnValue(wltparams)

class AgentPage(LiveMainPage):
    defaultView = ['load']
    
    def render_content(self, ctx, data):
        role = self.getRole(ctx)
        
        if role == 0:
            return self.renderAlert('You are not authorized')
        
        return loaders.xmlfile('webapp/agent/index.html')
    
    @inlineCallbacks
    def createClientList(self, ctx, data):      
        session = inevow.ISession(ctx)  
        states = {0: 'new.png',
                  1: 'connected.png',
                  2: 'established.png',
                  3: 'dead.png'}
        
        clTable = ClientTable(self, session.uid)
        
        clientList = yield session.agent.rootAgent.listClients()
        
        for client in clientList:
            imgState = '/images/cl-status/' + states.get(client.state, 'new.png')
            
            clTable.add({'id': client.id, 
                         'uuid': client.uuid, 
                         'class': client.type, 
                         'state': imgState, 
                         'endpoint': client.endpoint})
            
        clTable.setPage(0)
        
        returnValue(clTable)
    
    @inlineCallbacks
    def createLoadAgentList(self, ctx, data):
        session = inevow.ISession(ctx) 
        
        laTable = LoadAgentTable(self, session.uid)
        
        agentList = yield session.agent.expsvcAgent.listAgents()
        
        for agentUuid, agentInfo in agentList.iteritems():
            imgState = '/images/cl-status/'
            imgState += 'established.png' if agentInfo.isOnline else 'dead.png'
            
            agentHref = '/agent/load/' + agentUuid
             
            lowerOsName = agentInfo.osname.lower()
            
            osIcon = 'unknown.png'
            for needle, icon in osIcons:
                if needle in lowerOsName:
                    osIcon = icon
                    break
            osIcon = '/images/os-icons/' + osIcon
            
            laTable.add({'id': agentInfo.agentId,
                         'state': imgState, 
                         'hostname': agentInfo.hostname,
                         'agentHref': agentHref,
                         'osIcon': osIcon,
                         'osname': agentInfo.osname,
                         'release': agentInfo.release,
                         'arch': agentInfo.machineArch,
                         'cpus': agentInfo.numCPUs,
                         'cores': agentInfo.numCores,
                         'memtotal': agentInfo.memTotal})
        
        
        laTable.setPage(0)
        
        returnValue(laTable)
    
    def render_mainView(self, ctx, data):
        request = inevow.IRequest(ctx)
        
        what = request.args.get('what', self.defaultView)
        
        if 'client' in what:
            return self.createClientList(ctx, data)
        elif 'load' in what:
            return self.createLoadAgentList(ctx, data)
        
        return ''
    
    def render_agentMenu(self, ctx, data):
        menu = AgentMenu()
        role = self.getRole(ctx)
        
        request = inevow.IRequest(ctx)  
        what = request.args.get('what', self.defaultView)[0]
        menu.setActiveWhat(what)
        
        menu.addAgentItem('Loader agents', 'load')
        menu.addAgentItem('Monitor agents', 'monitor')
        
        if role == TSUserDescriptor.AUTH_ADMIN:
            menu.addAgentItem('Clients', 'client')
        
        return menu
    
    def child_load(self, ctx):
        return LoadAgentInfoPage()