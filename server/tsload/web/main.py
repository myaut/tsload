
'''
    This file is part of TSLoad.
    Copyright 2013, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
'''  



from tsload.web import TSWebAgent, webappPath

from twisted.python import util

from nevow import inevow
from nevow import loaders
from nevow import rend
from nevow import url
from nevow import livepage
from nevow import tags as T

from tsload.jsonts.api.user import TSUserDescriptor

class Menu(rend.Fragment):
    docFactory = loaders.xmlfile(webappPath('menu.html'))  
    
    navClass = 'nav'
    
    def __init__(self):
        self.data_menuItems = []
        
        rend.Fragment.__init__(self)
    
    def render_navPanel(self, ctx, data):
        # Intermediate renderer that changes navClass
        # Doesn't know, how to make nevow:attr work here :(
        ctx.tag(_class = self.navClass)
        return self.render_sequence(ctx, data)
    
    def addItem(self, title, url, isActive = False, isDisabled = False):
        liClass = 'active' if isActive else 'disabled' if isDisabled else '' 
        
        self.data_menuItems.append({'title': title, 
                                    'url': url,
                                    'liClass': liClass})
    
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
                
        self.userName = 'Guest'
        
        rend.Fragment.__init__(self)
        
    def setUserName(self, userName):
        self.userName = userName
        
    def render_menu(self, ctx, data):
        return self.menu
    
    def render_userName(self, ctx, data):
        return self.userName

class MainMenu(rend.Fragment):
    docFactory = loaders.xmlfile(webappPath('mainmenu.html'))
    
    def __init__(self):
        self.menu = Menu()
        
        rend.Fragment.__init__(self)
        
    def render_menu(self, ctx, data):
        return self.menu

class MainPageMixin:
    def getRole(self, ctx):
        session = inevow.ISession(ctx)
        role = getattr(session, 'userRole', 0)
        
        return role
    
    def renderAlert(self, alertMsg):
        return T.div[
                   T.div(_class='alert alert-error')[alertMsg],
               ]
    
    def render_topMenu(self, ctx, data):
        session = inevow.ISession(ctx)
        topMenu = TopMenu()
        
        currentPath = url.URL.fromContext(ctx).pathList()[0]
        
        topMenu.menu.addItem('About', '/about', currentPath == 'about')
        
        try:
            agent = session.agent
            userName = session.userName
        except AttributeError:
            topMenu.menu.addItem('Log in', '/login', currentPath == 'login')
        else:
            topMenu.menu.addItem('Log out', '/logout')
            topMenu.setUserName(userName)
            
        return topMenu
    
    def render_mainMenu(self, ctx, data):
        role = self.getRole(ctx)        
        if role == 0:
            return ''
        
        currentPath = url.URL.fromContext(ctx).pathList()[0]
        
        mainMenu = MainMenu()
        mainMenuItems = [('Agents', '/agent'),
                         ('Profiles', '/profile'),
                         ('Experiments', '/experiment'),
                         ('Monitoring', '/monitoring')]
        
        if role == TSUserDescriptor.AUTH_ADMIN:
            mainMenuItems.append(('Users', '/users'))
        
        for title, href in mainMenuItems: 
            mainMenu.menu.addItem(title, href, currentPath == href[1:])
            
        return mainMenu
    
    def render_content(self, ctx, data):
        return loaders.xmlfile(webappPath('main.html'))
    
    def render_CSS(self, ctx, data):
        return ''

class MainPage(rend.Page, MainPageMixin):
    docFactory = loaders.xmlfile(webappPath('index.html'))    
    
    def render___liveglue(self, ctx, data):
        return ''
    
class LiveMainPage(livepage.LivePage, MainPageMixin):
    docFactory = loaders.xmlfile(webappPath('index.html'))    
    
    def render___liveglue(self, ctx, data):
        return T.directive('liveglue')

class AboutPage(MainPage):
    def render_content(self, ctx, data):
        return loaders.xmlfile(webappPath('about.html'))
