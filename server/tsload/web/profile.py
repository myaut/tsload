'''
Created on Sep 25, 2013

@author: myaut
'''

import time

from nevow import livepage
from nevow import loaders
from nevow import url
from nevow import inevow
from nevow import tags as T

from twisted.internet.defer import inlineCallbacks, returnValue

from tsload.web import webappPath
from tsload.web.main import MainPage, LiveMainPage
from tsload.web.pagination import PaginatedView

from tsload.jsonts import tstimeToUnixTime
from tsload.jsonts.api.user import TSUserDescriptor
from tsload.jsonts.api.expsvc import *

from tsload.wlparam import WLParamHelper
from tsload.util.format import tstimeToHuman, sizeToHuman

# FIXME: Should be cleaned periodically
profilePagesCache = {}

class ProfileTable(PaginatedView):
    elementsPerPage = 20
    searchPlaceholder = 'Enter profile name...'
    paginatedDataField = 'data_profiles'
    
    def render_mainTable(self, ctx, data):
        return loaders.xmlfile(webappPath('profile/index.html'))
    
    def doFilter(self, criteria, row):
        return row['name'] == criteria
    
    def render_customControls(self, ctx, data):
        return T.a(_class = 'btn btn-success btn-large',
                   href='/profile/create')['Create profile']

class ProfileConfigurationPage(LiveMainPage):
    @staticmethod
    def createProfilePage(userId, profileName):
        if (userId, profileName) not in profilePagesCache:
            profilePage = ProfileConfigurationPage()
            profilePage.userId = userId
            profilePage.profileName = profileName
            
            profilePagesCache[userId, profileName] = profilePage
            
            return profilePage
        
        return profilePagesCache[userId, profileName]
    
    @inlineCallbacks
    def render_content(self, ctx, data): 
        session = inevow.ISession(ctx)
        
        self.data_threadpools = []
        self.data_workloads = []
        
        profile = TSExperimentProfile.createEmptyProfile()        
        profile.userId = self.userId
        
        profile = yield session.agent.expsvcAgent.getProfile(profileName = self.profileName,
                                                             profile = profile)
        
        ctx.fillSlots('profileName', self.profileName)
        ctx.fillSlots('userId', profile.userId)
        ctx.fillSlots('description', profile.description)
        
        creationDate = time.ctime(tstimeToUnixTime(profile.creationDate))
        ctx.fillSlots('creationDate', creationDate)
        
        self.agentList = yield session.agent.expsvcAgent.listAgents()
        
        # XXX: agentList uses uuids as index, JSON-TS usually have name (hostname) to object maps,
        # and we need agentId. Possibly listAgents have to be redesigned.
        urlRoot = url.URL.fromContext(ctx).click('/')
        agentRoot = urlRoot.child('agent').child('load')
        
        def _agentInfo(agentId):
            for agentUuid, agent in self.agentList.iteritems():
                if agent.agentId == agentId:
                    agentName = '%s (%d)' % (agent.hostname, agent.agentId)
                    agentHref = agentRoot.child(agentUuid) 
                                        
                    return agentName, agentHref
                    
            return '', '#'
        
        for threadpoolName, threadpool in profile.threadpools.iteritems():
            agentName, agentHref = _agentInfo(threadpool.agentId)
            
            self.data_threadpools.append({'name': threadpoolName,
                                          'agentName': agentName,
                                          'agentHref': agentHref,
                                          'numWorkers': str(threadpool.numWorkers)})
            
        for workloadName, workload in profile.workloads.iteritems():
            agentName, agentHref = _agentInfo(threadpool.agentId)
            
            workloadType = '' if workload.workloadType is None else workload.workloadType
            workloadTypeHref = agentHref.add('what', 'wltype').add('wlt', workloadType)
            
            threadpool = '' if workload.threadpool is None else workload.threadpool
            
            workloadHref = url.URL.fromContext(ctx).click(workloadName)
            
            self.data_workloads.append({'name': workloadName,
                                        'workloadHref': workloadHref,
                                        'agentName': agentName,
                                        'agentHref': agentHref,
                                        'workloadType': workloadType,
                                        'workloadTypeHref': workloadTypeHref,
                                        'threadpool': threadpool})
                                                
        
        self.profile = profile
        
        returnValue(loaders.xmlfile(webappPath('profile/info.html')))

class ProfilePage(MainPage):
    def locateChild(self, ctx, segments):
        if len(segments) == 2:
            userId = int(segments[0])
            profileName = segments[1]
            
            profilePage = ProfileConfigurationPage.createProfilePage(userId, 
                                                                     profileName)
            
            return (profilePage, [])
        
        return MainPage.locateChild(self, ctx, segments[1:])
    
    @inlineCallbacks
    def render_content(self, ctx, data):        
        session = inevow.ISession(ctx)
        role = self.getRole(ctx)
        
        if role == 0:
            returnValue(self.renderAlert('You are not authorized'))    
         
        profileTable = ProfileTable(self, session.uid)
        
        profiles = yield session.agent.expsvcAgent.listProfiles()
        
        for profileName, profile in profiles.iteritems():
            creationDate = time.ctime(tstimeToUnixTime(profile.creationDate))
            
            profileHref = url.URL.fromContext(ctx).child(str(profile.userId)).child(str(profileName))
            
            profileTable.add({'profileId': str(profile.profileId),
                              'userId': str(profile.userId),
                              'name': profileName,
                              'profileHref': profileHref,
                              'creationDate': creationDate,
                              'description': profile.description})
        
        profileTable.setPage(0)
        
        returnValue(profileTable)
        