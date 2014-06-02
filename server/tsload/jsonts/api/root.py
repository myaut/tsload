
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

class TSHelloResponse(tso):
    agentId = tso.Int()
    
class TSClientDescriptor(tso):    
    id = tso.Int()
    type = tso.String()
    uuid = tso.String()
    authType = tso.Int()
    
    # See JSONTS.STATE_*
    state = tso.Int()
    
    endpoint = tso.String()

class RootAgent(TSAgentInterface):
    hello = TSMethod(tso.Object(TSHelloResponse),
                     agentType = tso.String(),
                     agentUuid = tso.String())
    
    authMasterKey = TSMethod(masterKey = tso.String())
    
    listClients = TSMethod(tso.Array(tso.Object(TSClientDescriptor)))

