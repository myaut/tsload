'''
Created on 02.06.2013

@author: myaut
'''

from nevow import livepage
from nevow import loaders
from nevow import rend
from nevow import inevow
from nevow import url
from nevow import tags as T

from twisted.internet.defer import inlineCallbacks, returnValue

from tsload.web.main import MainPage, LiveMainPage, Menu
from tsload.web.pagination import PaginatedView

from tsload.jsonts.api.user import TSUserDescriptor

osIcons = [('debian', 'debian.png'),
           ('suse', 'suse.png'),
           ('rhel', 'redhat.png'),
           ('centos', 'redhat.png'),
           ('ubuntu', 'ubuntu.png'),
           ('solaris', 'solaris.png'),
           ('windows', 'windows.png')]

class AgentMenu(Menu):
    navClass = 'nav nav-tabs'
    
    def setActiveWhat(self, what):
        self.actWhat = what
    
    def addAgentItem(self, title, what):
        self.addItem(title, 
                     url.here.click('?what=' + what),
                     self.actWhat == what)

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

class LoadAgentInfoPage(MainPage):
    defaultView = ['agent']
    agentUuid = ''
    
    def locateChild(self, ctx, segments):
        if segments:
            self.agentUuid = segments[0]
        
        return (self, [])
    
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
                break
        else:
            self.clientInfo = None
    
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
        
        menu.addAgentItem('Information', 'agent')
        menu.addAgentItem('Agent resources', 'resource')
        menu.addAgentItem('Workload types', 'wltype')
        menu.addAgentItem('Experiments', 'experiments')
        
        return menu
    
    def render_agentName(self, ctx, data):
        return self.agentInfo.hostname
        
    def render_agentInfo(self, ctx, data):
        if not self.agentUuid:
            return url.here.up()
        
        request = inevow.IRequest(ctx)
        
        what = request.args.get('what', self.defaultView)
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
        
        if 'agent' in what:
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
        
        return ''

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