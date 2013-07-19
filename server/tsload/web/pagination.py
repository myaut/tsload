'''
Created on Jun 10, 2013

@author: myaut
'''

from tsload.web import webappPath

from nevow import livepage
from nevow import loaders
from nevow import rend
from nevow import flat
from nevow import url
from nevow import inevow
from nevow import tags as T

from functools import partial

class PaginatedView(rend.Fragment):
    '''Provides paginated and filtered view of sequencable data. 
    
    rawData is kept in rawData array. If no filter were invoked,
    filteredData simply references this list. If doFilter called,
    new list is constructed. 
    
    After each change of page, filteredData is spliced into 
    paginatedData and field named data_* (based on paginatedDataField)
    set for Nevow sequence renderer.
    
    It is also needs small fix to liveglue.js:
        var anchorIndex = base_url.indexOf('#');
        if (anchorIndex != -1) {
          base_url = base_url.substring(0, anchorIndex);
         }
    
    Have three class variables:
        elementsPerPage - number of elements set on page
        paginatedDataField - name of data field used by nevow renderer to render mainTable
        searchPlaceholder - label shown in search field until user start typing query
    '''
    
    elementsPerPage = 20
    paginatedDataField = 'data_default'
    searchPlaceholder = 'Search...'
    
    docFactory = loaders.xmlfile(webappPath('pageview.html'))
    
    def __init__(self, parent, sessionUid):
        '''Initialize table view
        
        @param parent: parent page that owns that view
        @param sessionUid: uid of session (needed to check if liveclient request are not fake)
        '''
        self.rawData = []
        self.prefilteredData = self.rawData
        self.filteredData = self.prefilteredData
        self.paginatedData = []
        
        self.pageId = 0
        
        self.sessionUid = sessionUid
        
        # Livepage seeks for page/handlers inside Page, not fragment, so add references
        # XXX: May be we should simply play with contexts?
        parent.handle_page = self.handle_page
        parent.handle_search = self.handle_search
        self.parent = parent
        
        rend.Fragment.__init__(self)
        
    def add(self, row):
        '''Append data row to rawData'''
        self.rawData.append(row)
    
    def getNumPages(self):
        '''Returns number of pages in view'''
        dataLength = len(self.filteredData)
        
        lastPage = dataLength % self.elementsPerPage != 0
        lastPageNum = 1 if lastPage else 0
        
        return dataLength / self.elementsPerPage + lastPageNum
    
    def setPage(self, pageId):
        self.pageId = pageId
        
        start = pageId * self.elementsPerPage
        end = min(start + self.elementsPerPage, len(self.filteredData))
        
        self.paginatedData = self.filteredData[start:end]
        
        # Add reference for Nevow renderer
        setattr(self.parent, self.paginatedDataField, self.paginatedData)
        setattr(self, self.paginatedDataField, self.paginatedData)
    
    def render_searchForm(self, ctx, data):
        return T.div[T.span(_class='label label-info', style='visibility: hidden', id='searchNotification'),
                     '  ',
                     ctx.tag(onkeyup=livepage.server.handle('search',
                                                            livepage.get('searchForm').value),
                             placeholder=self.searchPlaceholder)]
    
    def handle_search(self, ctx, query):
        if query == '':
            self.filteredData = self.prefilteredData
            yield self._searchNotify('')
        else:
            filterF = partial(self.doFilter, query)
            self.filteredData = filter(filterF, self.prefilteredData)
            
            if len(self.filteredData) == 0:
                self.filteredData = self.prefilteredData
                yield self._searchNotify('No results found')
            else:
                yield self._searchNotify('Found %s results' % len(self.filteredData))
        
        self.setPage(0)
        
        yield self.update(ctx)
    
    def _searchNotify(self, notification):
        visibility = 'visible' if notification else 'hidden'
        visJS = "document.getElementById('searchNotification').style.visibility='%s'" % visibility
        
        yield livepage.set('searchNotification', notification), livepage.eol
        yield livepage.js(visJS), livepage.eol
    
    def render_pagination(self, ctx, data):
        pagin = T.ul(id='pagination')
        
        numPages = self.getNumPages()
        
        request = inevow.IRequest(ctx)
        href = str(request.URLPath())
        
        def createPageLink(pageId, pageTitle, isActive=False):
            _ = pagin[T.li(_class = 'active' if isActive else '')[
                           T.a(onclick=livepage.server.handle('page', pageId),
                               href='#')[pageTitle]]]
        
        firstPage = max(self.pageId - 1, 0)
        lastPage = min(firstPage + 3, numPages)
        
        # print 'first: %d last: %d num: %d cur: %d' % (firstPage, lastPage, numPages, self.pageId)
        
        if firstPage > 0:
            createPageLink(0, '<<')
        if self.pageId > 0:
            createPageLink(self.pageId - 1, '<')
        
        pageList = map(lambda p: (p, str(p+1)), range(firstPage, lastPage))
        
        for pageId, pageTitle in pageList:
            createPageLink(pageId, pageTitle, pageId == self.pageId)
        
        if self.pageId < (numPages - 1):
            createPageLink(self.pageId + 1, '>')
        if lastPage < numPages:
            createPageLink(numPages, '>>')
        
        return pagin
    
    def handle_page(self, ctx, pageId):
        pageId = int(pageId)
        self.setPage(pageId)
        
        yield self.update(ctx)
    
    def update(self, ctx):
        '''Emit paginated view update'''
        session = inevow.ISession(ctx)
        
        # Verify if live request came from same session, our view was created
        # Nevow doesn't do it :(
        if session.uid == self.sessionUid:
            # nevow.livepage has problems with flattening complex objects
            # so render it into string prematurely
            flt = flat.flatten(self.render_mainTable(ctx, None), ctx)
            
            yield livepage.set('pagination', self.render_pagination(ctx, None)), livepage.eol
            yield livepage.set('mainTable', flt) 
    
    def doFilter(self, criteria, row):
        '''Generic method that applies filter to table'''
        return True
    
    def render_mainTable(self, ctx, data):
        '''Generic method that renders main table'''
        return ''
    
    def render_customControls(self, ctx, data):
        '''Generic method that renders custom controls (i.e. various filters)'''
        return ''