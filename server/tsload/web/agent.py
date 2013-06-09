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

from tsload.web.main import LiveMainPage, Menu
from tsload.web.pagination import PaginatedView

from tsload.jsonts.api.user import TSUserDescriptor

class AgentMenu(Menu):
    navClass = 'nav nav-tabs'
    
    def setActiveWhat(self, what):
        self.actWhat = what
    
    def addAgentItem(self, title, what):
        self.addItem(title, 
                     url.here.click('?what=' + what),
                     self.actWhat == what)

class ClientTable(PaginatedView):
    elementsPerPage = 20
    searchPlaceholder = 'Enter client type...'
    paginatedDataField = 'data_clients'
    
    def render_mainTable(self, ctx, data):
        return loaders.xmlfile('webapp/agent/client.html')
    
    def doFilter(self, criteria, row):
        return row['class'] == criteria
    
class AgentPage(LiveMainPage):
    defaultView = ['client']
    
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
    
    def render_mainView(self, ctx, data):
        request = inevow.IRequest(ctx)
        
        what = request.args.get('what', self.defaultView)
        
        if 'client' in what:
            @inlineCallbacks
            def createClientList(ctx, data):
                clTable = yield self.createClientList(ctx, data)
                clTable.page = self
                
                returnValue(clTable)
            return createClientList
        
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