'''
Created on 10.05.2013

@author: myaut
'''

from tsload.web import TSWebAgent

from twisted.python import util

from nevow import inevow
from nevow import loaders
from nevow import rend
from nevow import tags as T

class Menu(rend.Fragment):
    docFactory = loaders.htmlstr("""
    <ul nevow:data="menuItems" nevow:render="sequence" class="nav">
        <li nevow:pattern="item" nevow:render="mapping">
            <a href="#">
                <nevow:attr name="href" nevow:render="data" nevow:data="url">
                    <nevow:slot name="url">#</nevow:slot>
                </nevow:attr>
                <span><nevow:slot name="title">Title</nevow:slot></span>
            </a>
        </li>
    </ul>
    """)
    
    def __init__(self):
        self.data_menuItems = []
        
        rend.Fragment.__init__(self)
    
    def addItem(self, title, url):
        self.data_menuItems.append({'title': title, 
                                    'url': url})
        
class TopMenu(rend.Fragment):
    docFactory = loaders.htmlstr("""
    <div class="pull-right">
        <ul class="nav">
            <li><a>Welcome, <span nevow:render="userName">Guest</span></a></li>
        </ul>
        <span nevow:render="menu">Menu</span>
    </div>
    """)
    
    def __init__(self):
        self.menu = Menu()
                
        self.menu.addItem('About', '/about')
        self.userName = 'Guest'
        
    def setUserName(self, userName):
        self.userName = userName
        
    def render_menu(self, ctx, data):
        return self.menu
    
    def render_userName(self, ctx, data):
        return self.userName

class MainPage(rend.Page):
    docFactory = loaders.xmlfile('webapp/index.html')
    
    def render_topMenu(self, ctx, data):
        session = inevow.ISession(ctx)
        topMenu = TopMenu()
        
        try:
            agent = session.agent
            userName = session.userName
        except AttributeError:
            topMenu.menu.addItem('Log in', '/login')
        else:
            topMenu.menu.addItem('Log out', '/logout')
            topMenu.setUserName(userName)
            
        return topMenu
    
    def render_mainMenu(self, ctx, data):
        return ''
    
    def render_content(self, ctx, data):
        return loaders.xmlfile('webapp/main.html')
    
    def render_CSS(self, ctx, data):
        return ''

class AboutPage(MainPage):
    def render_content(self, ctx, data):
        return loaders.xmlfile('webapp/about.html')