
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



from tsload import config

from tsload.web import TSWebAgent, webappPath

from nevow import rend
from nevow import loaders
from nevow import inevow
from nevow import url
from nevow import tags as T

from nevow.util import Deferred

from formless import annotate
from formless import webform
from formless import configurable 

from zope.interface import implements

from tsload.web.main import MainPage

class ILoginForm(annotate.TypedInterface):
    def login(ctx = annotate.Context(),
        userName = annotate.String(required=True, requiredFailMessage='Please enter your name'),
        password = annotate.PasswordEntry(required=True, requiredFailMessage='Please enter your name')):
            pass
    annotate.autocallable(login)

class LoginFailurePage(MainPage):
    errorMessage = ''
    
    def render_content(self):
        return 
        
class LoginPage(MainPage):    
    implements(ILoginForm)
    
    def render_content(self, ctx, data):
        return loaders.xmlfile(webappPath('login.html'))
        
    def render_loginFailure(self, ctx, data):
        session = inevow.ISession(ctx)
        
        if getattr(session, 'loginFailure', ''):
            ret = T.div[
                     T.div(_class='alert alert-error')[session.loginFailure],
                     ]
            del session.loginFailure
            return ret
        
        return ''
    
    def render_loginForm(self, loginForm):        
        return webform.renderForms()
    
    def render_CSS(self, ctx, data):
        return file(webappPath('css/login.css')).read()
    
    def loginResponse(self, agent, session):
        if agent.state != TSWebAgent.STATE_AUTHENTIFICATED:
            if agent.state == TSWebAgent.STATE_CONN_ERROR:            
                session.loginFailure = 'Connection Error: ' + agent.connectionError
            elif agent.state == TSWebAgent.STATE_AUTH_ERROR:            
                session.loginFailure = 'Authentification Error: ' + agent.authError
            else:
                session.loginFailure = 'Invalid agent state'
            
            del session.agent
            
            return ''
        else:
            # Authentification OK - return to main page
            session.userName = session.agent.gecosUserName
            session.userRole = session.agent.userRole
            
            return url.here.up()
    
    def login(self, ctx, userName, password):
        session = inevow.ISession(ctx)
        
        session.agent = TSWebAgent.createAgent('web', 
                                               config.get('tsserver', 'host'),
                                               config.getInt('tsserver', 'port'))
        session.agent.setAuthData(userName, password)
        
        d = Deferred()
        d.addCallback(self.loginResponse, session)
        
        session.agent.setEventListener(d)
        
        return d
    
class LogoutPage(rend.Page):
    def renderHTTP(self, context):
        session = inevow.ISession(context)
        
        try:
            delattr(session, 'agent')
            delattr(session, 'userName')
            delattr(session, 'userRole')
        except AttributeError:
            pass
        
        return url.here.up()

        
