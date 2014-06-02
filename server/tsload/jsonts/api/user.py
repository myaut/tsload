
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



from tsload.jsonts.object import TSObject as tso

from tsload.jsonts.api import TSAgentInterface, TSMethod

class TSUserDescriptor(tso):
    name = tso.String()
    
    # See tsload.jsonts.server.TSServerClient - AUTH_* contstants
    AUTH_ADMIN = 2
    AUTH_OPERATOR = 3
    AUTH_USER = 4
    
    role = tso.Int()

class UserAgent(TSAgentInterface):
    authUser = TSMethod(tso.Object(TSUserDescriptor),
                        userName = tso.String(),
                        userPassword = tso.String())
