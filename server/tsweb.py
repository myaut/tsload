
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



from tsload import config, logging

from twisted.internet import reactor

from nevow.appserver import NevowSite
from nevow.static import File

config.readConfig('web.cfg')
config.setWorkDir('tsweb')

logging.initLogging()

from tsload.web import webappPath
from tsload.web.main import MainPage, AboutPage
from tsload.web.login import LoginPage, LogoutPage
from tsload.web.agent import AgentPage
from tsload.web.profile import ProfilePage

main = MainPage()

# Static directories
main.putChild('bootstrap', File(webappPath('bootstrap')))
main.putChild('css', File(webappPath('css')))
main.putChild('js', File(webappPath('js')))
main.putChild('images', File(webappPath('images')))

# Pages
main.putChild('about', AboutPage())
main.putChild('login', LoginPage())
main.putChild('logout', LogoutPage())

main.putChild('agent', AgentPage())
main.putChild('profile', ProfilePage())

site = NevowSite(main, logPath = config.get('logging', 'logaccess'))

port = config.getInt('tsweb', 'port')
logging.getLogger('__main__').info('Starting TSLoad Web Console instance at http port %d...' % port)

reactor.listenTCP(port, site)
reactor.run()
