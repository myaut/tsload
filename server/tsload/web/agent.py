'''
Created on 02.06.2013

@author: myaut
'''

from nevow import rend
from nevow import loaders
from nevow import inevow
from nevow import url
from nevow import tags as T

from tsload.web.main import MainPage, Menu

class AgentMenu(Menu):
    nav = 'nav nav-pills'

class AgentPage(MainPage):
    def render_content(self, ctx, data):
        role = self.getRole(ctx)
        
        if role == 0:
            return T.div[
                       T.div(_class='alert alert-error')['You are not authorized to get here'],
                   ]
        
        return loaders.xmlfile('webapp/agent/index.html')
    
    def render_agentMenu(self, ctx, data):
        menu = AgentMenu()
        
        menu.addItem('Loader agents', '/loader')
        
        return menu