'''
Created on 10.05.2013

@author: myaut
'''

from twisted.internet import reactor

from nevow.appserver import NevowSite
from nevow.static import File

from tsload.web.main import MainPage, AboutPage
from tsload.web.login import LoginPage, LogoutPage

main = MainPage()

# Static directories
main.putChild('bootstrap', File('webapp/bootstrap'))
main.putChild('css', File('webapp/css'))
main.putChild('js', File('webapp/js'))
main.putChild('images', File('webapp/images'))

# Pages
main.putChild('about', AboutPage())
main.putChild('login', LoginPage())
main.putChild('logout', LogoutPage())

site = NevowSite(main)

reactor.listenTCP(8080, site)
reactor.run()