
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



from tsload.web import webappPath

from nevow import livepage
from nevow import loaders
from nevow import rend
from nevow import flat
from nevow import url
from nevow import inevow
from nevow import tags as T

from copy import copy

class TreeViewElement:
    def __init__(self):
        self.children = []
        self.parent = None
    
    def addChild(self, child):
        if child.parent is not None:
            child = copy(child)
            
        self.children.append(child)
        child.parent = self
    
    def render(self):
        '''Generic render function'''
        return repr(self)
    
    def doRender(self):
        if not self.children:
            _class = 'icon-leaf'
        else:
            if self.parent:
                _class = 'icon-minus-sign'
            else:
                _class = 'icon-folder-open'
        
        this_tag = T.li[T.span[T.i(_class = _class),
                               self.render()]]
        
        if self.children:
            list_tag = T.ul()
            this_tag = this_tag[list_tag]
            children_tags = [child.doRender()
                             for child 
                             in self.children]
            
            list_tag[children_tags]
            
        return this_tag
            
class TreeView(rend.Fragment):
    searchPlaceholder = ''
    
    # TODO: Dynamic filters like in Paginated view
    # TODO: Collapse/Expand all
    
    docFactory = loaders.xmlfile(webappPath('treeview.html'))
    
    def __init__(self, parent, sessionUid):
        self.elementList = []
        self.filteredElementList = self.elementList
        
        self.sessionUid = sessionUid
    
    def addElement(self, element):
        self.elementList.append(element)
    
    def render_searchForm(self, ctx, data):
        return T.div[ctx.tag(onkeyup=livepage.server.handle('search',
                                                            livepage.get('searchForm').value),
                             placeholder=self.searchPlaceholder)]
    
    def handle_search(self, ctx, query):
        pass
    
    def render_treeView(self, ctx, data):
        root_tags = [element.doRender()
                     for element 
                     in self.filteredElementList
                     if element.parent is None]
        
        return T.div(_class = 'tree well')[T.ul[root_tags]]
    
    def render_customControls(self, ctx, data):
        '''Generic method that renders custom controls (i.e. various filters)'''
        return ''
