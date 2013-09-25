'''
Created on 10.05.2013

@author: myaut
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