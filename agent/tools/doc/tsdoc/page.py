import os
import sys

from pprint import pprint

from collections import defaultdict

from tsdoc import *
from tsdoc.blocks import *
from tsdoc.mdparser import MarkdownParser

_TRACE_DOCS = []

class TSDocProcessError(Exception):
    pass

class DocPage(object):
    HEADER_HSIZE = 1
    
    def __init__(self, docspace, name):
        self.docspace = docspace
        self.name = name
        
        self.header = ''
        
        self.page_path = '%s/%s' % (docspace, name)
        
        self.blocks = []
        self.references = {}
        
        self.nav_links = {}
        
        self.doc_path = None
    
    def __iter__(self):
        return iter(self.blocks)
    
    def __repr__(self):
        return '<%s %s/%s>' % (self.__class__.__name__,
                               self.docspace, self.name)
    
    def create_doc_path(self, doc_dir, doc_suffix):
        doc_path = os.path.join(doc_dir, self.docspace,
                                self.name + doc_suffix)     
        self.doc_path = doc_path
    
    def prep_print(self):
        if self.header:
            hobj = Header(self.header, DocPage.HEADER_HSIZE)
            self.blocks.insert(0, Block([hobj]))
    
    def set_format(self, doc_format):
        self.format = doc_format
        
    def iter_parts(self, pred):
        ''' Generator - iteratively other all blocks/parts 
        and returns those that match predicate. 
        
        Allows to avoid recursive algorithms'''
        return self._iter_parts_block(pred, self.blocks) 
    
    def _iter_parts_block(self, pred, block, level = 0):
        for part in block:
            if pred(part):
                yield part
            
            if isinstance(part, Block):
                parts = self._iter_parts_block(pred, part, level + 1)
                for block_part in parts:
                    yield block_part
        
    
    def add_nav_link(self, nav_type, page):
        where = self.gen_link_to(page)
        self.nav_links[nav_type] = NavLink(nav_type, page, where)
        
    def gen_link_to(self, page):        
        doc_dir = os.path.dirname(self.doc_path)
        return os.path.relpath(page.doc_path, doc_dir)

class MarkdownPage(DocPage):
    def __init__(self, page_path):
        path, name = os.path.split(page_path)
        _, docspace = os.path.split(path)
        
        name = name[:name.find('.')]
        
        DocPage.__init__(self, docspace, name)
        
        fp = file(page_path, 'r')
        text = fp.read()
        fp.close()
        
        parser = MarkdownParser(text, self.      page_path)
        self.blocks = parser.parse()

class TSDocPage(DocPage):
    ref_types = defaultdict(list)
    
    CLASS_HSIZE = 2
    GROUP_HSIZE = 3
    
    CLASS_HEADERS = [None,
                     'Variables',
                     'Constants',
                     'Functions',
                     'Types']
    
    DEF_NAME_VARIABLE = 'Variable'
    DEF_NAME_CONSTANT = 'Constant'
    DEF_NAME_TYPEDEF = 'Typedef'
    DEF_NAME_FUNCTION = 'Function'
    DEF_NAME_MACRO = 'Macro'
    DEF_NAME_STRUCT = 'Struct'
    DEF_NAME_UNION = 'Union'
    DEF_NAME_ENUM = 'Enum'
    
    LABEL_PUBLIC = ('success', 'public')
    LABEL_PRIVATE = ('warning', 'private')
    LABEL_PLAT = ('info', 'plat')
    
    DOCTEXT_NOTES = [(DocText.Note.RETURN, 'RETURN VALUES'),
                     (DocText.Note.NOTE, 'NOTES'),
                     (DocText.Note.REFERENCE, 'REFERENCE')]
    
    DOCTEXT_PARAMS = [(DocText.Param.ARGUMENT, 'ARGUMENTS'),
                      (DocText.Param.VALUE, 'VALUES'),
                      (DocText.Param.MEMBER, 'MEMBERS')]
    
    def __init__(self, page_path):    
        self.tsdoc = self._load(page_path)
        
        DocPage.__init__(self, 
                         self.tsdoc.docspace, 
                         self.tsdoc.module)
        self.header = self.tsdoc.header
        
        self._preprocess()
    
    def _load(self, page_path):
        fp = file(page_path, 'r')
        obj = json.load(fp)
        
        tsdoc = TSDoc.deserialize(obj)
        
        return tsdoc
    
    def _preprocess(self):
        self.group_map = {}
        self.ref_list = TSDocPage.ref_types[self.tsdoc.docspace]
        
        pub_count = 0
        
        # 1st pass: map groups to their names
        for group in self.tsdoc.groups:
            for name in group.get_names():
                if name in self.group_map:
                    # Compare weights of groups
                    other = self.group_map[name]
                    if group.get_weight() < other.get_weight():
                        continue
                
                self.group_map[name] = group
        
        # 2nd pass: find public groups - they contain DocText,
        # have public function or referenced typename
        for group in self.tsdoc.groups:
            group.public = False
            group.labels = []
                        
            if group.have_doctext():
                group.public = True
            
            for defobj in group:                
                if isinstance(defobj, Function):
                    is_header = defobj.source.endswith('.h')
                    
                    if 'LIBEXPORT' in defobj.specifiers:
                        group.public = True
                        group.labels.append(TSDocPage.LABEL_PUBLIC)
                    if 'STATIC_INLINE' in defobj.specifiers or \
                       'static' in defobj.specifiers:
                            if is_header:
                                group.public = True
                                group.labels.append(TSDocPage.LABEL_PUBLIC)
                            else:
                                group.labels.append(TSDocPage.LABEL_PRIVATE)
                    if 'PLATAPI' in defobj.specifiers:
                        group.labels.append(TSDocPage.LABEL_PLAT)
                    
                    if not group.public:
                        continue
                    
                    # Walk referenced types but only for public functions
                    for arg in defobj.args:
                        for arg_type in arg.types:
                            self.ref_list.append(arg_type)
    
                    
    def process(self):
        group_table = [[], [], [], [], []]
        
        def _all_isinstance(klass, iterable):
            return all(isinstance(obj, klass) 
                       for obj in iterable) 
        
        # 3rd pass - identify global types
        for group in self.tsdoc.groups:
            if not group.public:
                for defobj in group:
                    if defobj.def_class == Definition.DEF_TYPE or   \
                            isinstance(defobj, Enumeration) and     \
                            defobj.name in self.ref_list:
                        group.public = True
                        break
            
            if group.public:
                leaders = group.find_leaders()
                group_class = max(defobj.def_class 
                                  for defobj in leaders)
                
                group_table[group_class].append(group)
        
        # 4nd pass - generate text
        for (header, groups) in zip(TSDocPage.CLASS_HEADERS,
                                    group_table):
            if header and groups:
                hobj = Header(header, TSDocPage.CLASS_HSIZE)
                self.blocks.append(Block([hobj]))
            
            for group in groups:
                code = []
                
                # Create header block - header of group,
                # references for cross-reference links
                header_block = Block()
                
                for defobj in group:
                    if defobj.name:
                        def_class_name = self._get_defclass_name(defobj)
                        
                        def_name = defobj.name
                        if isinstance(def_name, list):
                            def_name = ' '.join(def_name)
                        
                        header_block.add(CodeReference(def_name, 
                                                       def_name, 
                                                       def_class_name))
                
                header = group.header()
                hobj = Header(header, TSDocPage.GROUP_HSIZE)
                header_block.add(hobj)
                
                # Generate labels - small hints which show if
                # function is public (has LIBEXPORT or statically 
                # defined in header), or PLATAPI. 
                # group.labels assigned during 2nd pass of preprocess
                labels = []
                for (style, label) in group.labels:
                    if label not in labels:
                        header_block.add(Label(label, style))
                        labels.append(label)
                
                self.blocks.append(header_block)
                
                for defobj in group:
                    if isinstance(defobj, DocText):
                        self._process_doctext(defobj)
                    else:
                        if defobj.code:
                            if isinstance(defobj.code, list):
                                code.extend(defobj.code)
                            else:
                                code.append(defobj.code)
                            code.append('\n')
                
                # TODO: Xref types here
                if code:
                    code = ''.join(code)
                    self.blocks.append(Code([code]))

    def _process_doctext(self, doctext):
        block = Block()
        
        # First of all - notes that gone without any directive
        # They also is written on markdown, so parse them
        notes = doctext.get_notes(DocText.Note.TEXT)
        for note in notes:
            parser = MarkdownParser(note.note, self.page_path)
            
            if self.name in _TRACE_DOCS:
                parser.TRACE = True
            
            try:
                blocks = parser.parse() 
                block.extend(blocks)
            except MarkdownParser.ParseException as mpe:
                print >> sys.stderr, '%s/%s: %s' %(self.docspace, self.name, str(mpe))
                # Print as is without Markdown
                block.extend(note.note)
        
        # Add sections which created with @directives
        # Each section is a paragraph that begins with
        # ItalicText header
        
        # Process PARAMS/MEMBERS/VALUES
        # Generate unordered list for them
        for (type, header) in TSDocPage.DOCTEXT_PARAMS:
            params = doctext.get_params(type)
            
            if not params:
                continue
            
            param_block = Paragraph()
            param_block.add(ItalicText(header))
            param_block.add('\n')
            
            param_list = ListBlock()
            
            for param in params:
                entry = ListEntry(1, [BoldText(param.name),
                                      ' - ', param.description, '\n'])
                param_list.add(entry)
            
            param_block.add(param_list)
            block.add(param_block)
        
        # Then process other notes
        for (type, header) in TSDocPage.DOCTEXT_NOTES:
            notes = doctext.get_notes(type)
            
            if not notes:
                continue
            
            note_block = Paragraph()
            note_block.add(ItalicText(header))
            note_block.add('\n')
            
            for note in notes:
                note_block.extend([note.note, '\n'])
            
            block.add(note_block)
        
        self.blocks.append(block)
    
    def _get_defclass_name(self, defobj):
        if isinstance(defobj, TypeVar):
            if defobj.tv_class == TypeVar.TYPEDEF:
                return TSDocPage.DEF_NAME_TYPEDEF
            else:
                return TSDocPage.DEF_NAME_VARIABLE
        elif isinstance(defobj, Value):
            return TSDocPage.DEF_NAME_CONSTANT
        elif isinstance(defobj, Macro):
            return TSDocPage.DEF_NAME_MACRO
        elif isinstance(defobj, Function):
            return TSDocPage.DEF_NAME_FUNCTION
        elif isinstance(defobj, Enumeration):
            return TSDocPage.DEF_NAME_ENUM
        elif isinstance(defobj, ComplexType):
            if defobj.type == ComplexType.STRUCT:
                return TSDocPage.DEF_NAME_STRUCT
            else:
                return TSDocPage.DEF_NAME_UNION
        
        return 'Unknown'
        
class IndexPage(MarkdownPage):
    DOCSPACE_HSIZE = 3
    CHAPTER_HSIZE = 4
    REF_LETTER_HSIZE = 2
    
    INDEX_LINK_TEXT = 'Index'
    REFERENCE_LINK_TEXT = 'Reference'
    REFERENCE_HEADER = 'Reference'
    
    class DocSpaceReference(DocPage):
        def __init__(self, docspace, references):
            name = 'reference'
            
            DocPage.__init__(self, docspace, name)
            
            self.references = references
            self.header = IndexPage.REFERENCE_HEADER
            
            self.links = []
            
            self._preprocess()
        
        def _preprocess(self):            
            sorted_refs = defaultdict(dict)
            
            for (ref_link, ref_part) in self.references.items():
                if isinstance(ref_part, CodeReference):
                    name = ref_part.ref_name                    
                    first_letter = name[0].upper()
                    
                    sorted_refs[first_letter][name] = (ref_link, ref_part)
            
            for letter in sorted(sorted_refs.keys()):
                hobj = Header(letter, IndexPage.REF_LETTER_HSIZE)                
                block = Paragraph([hobj])
                
                refs = sorted_refs[letter]
                for name in sorted(refs.keys()):
                    ref_link, ref_part = refs[name]
                    
                    linkobj = Link(name, Link.INTERNAL, ref_link)
                    entry = [linkobj, ', ', ref_part.ref_class, '\n']
                    
                    block.extend(entry)
                    
                    self.links.append((self, linkobj))
                
                self.blocks.append(block) 
    
    class DocSpaceIndex(DocPage):
        def __init__(self, header, docspace, is_external, gen_reference):
            name = 'index'
            
            DocPage.__init__(self, docspace, name)
            self.header = header
            
            self.links = []
            
            self.is_external = is_external
            self.gen_reference = gen_reference
            
            self.reference = None
        
        def process(self, pages, docspaces):
            self.pages = pages
            self.docspaces = docspaces
            
            # Collect links and references from all pages
            for page in self.pages.values():
                links = self._collect_ref_links(page)
                self.links.extend(links)
            
            self.index_links = self._collect_ref_links(self)
            self.links.extend(self.index_links)
            
            # Generate reference
            if self.gen_reference:
                self._create_reference()
        
        def prepare(self, index):
            doc_dir = os.path.join(index.doc_dir, self.docspace)
            if not os.path.exists(doc_dir):
                os.makedirs(doc_dir)
            
            # Generate real paths 
            if self.is_external:
                self.pages['__index__'] = self
            else:
                # Main index file is real, so assume it's 
                # doc_path, so gen_link_to will think that it is 
                # located in build/doc not build/doc/<namespace>
                # and generate correct links
                self.doc_path = index.doc_path
            
            for page in self.pages.values():  
                page.prep_print()
                page.create_doc_path(index.doc_dir, index.doc_suffix)
                
        def generate(self, printer, index, joint = False):   
            pages = self.pages.values()   
            
            if self.is_external:
                self.add_nav_link(NavLink.HOME, index)
            
            # Generate navigation links
            self._gen_nav_links(self.index_links, index)
            
            # Substitute links with references to pages
            self._xref()
            
            for page in pages:
                print 'Generating %s...' % page.doc_path
                
                stream = file(page.doc_path, 'w')
                printer.do_print(stream, self.header, page)
        
        def find_page(self, link):
            docspace_name, name = link.where.split('/')            
            docspace = self
            
            # Cut anchor
            if '#' in name:
                name = name[:name.rfind('#')]
            
            if docspace_name != self.docspace:                  
                for docspace in self.docspaces:
                    if docspace_name == docspace.docspace:
                        break
                else:
                    print >> sys.stderr, 'WARNING: Invalid link "%s" in docspace "%s"' % (link.where,
                                                                                          self.docspace)
                    return None 
            
            try:
                return docspace.pages[name]
            except KeyError:
                print >> sys.stderr, 'WARNING: Link "%s" not found in docspace "%s"' % (link.where,
                                                                                        self.docspace)  
                return None
        
        def _collect_ref_links(self, page):
            def pred(part):
                return isinstance(part, Reference) or \
                       (isinstance(part, Link) and part.type == Link.INTERNAL)
            
            links = []
            
            iter_parts = page.iter_parts(pred)
            for part in iter_parts:
                if isinstance(part, Reference):
                    full_ref = '%s/%s#%s' % (page.docspace, page.name,
                                             part.text)
                    self.references[full_ref] = part
                else:
                    links.append((page, part))
            
            return links
        
        def _gen_nav_links(self, index_links, index):
            for (prev, cur, next) in zip([None] + index_links[:-1],
                                         index_links, 
                                         index_links[1:] + [None]):
                cur_page = self.find_page(cur[1])
                
                if cur_page is None:
                    continue
                
                if prev is not None:
                    prev_page = self.find_page(prev[1])
                    if prev_page is not None:
                        cur_page.add_nav_link(NavLink.PREV, prev_page)
                if next is not None:
                    next_page = self.find_page(next[1])
                    if next_page is not None:
                        cur_page.add_nav_link(NavLink.NEXT, next_page)
                
                if self.is_external:    
                    cur_page.add_nav_link(NavLink.UP, self)
                    
                if self.reference is not None:
                    cur_page.add_nav_link(NavLink.REF, self.reference)
                
                cur_page.add_nav_link(NavLink.HOME, index)
            
            if self.reference:
                if self.is_external:    
                    self.reference.add_nav_link(NavLink.UP, self)
                
                self.reference.add_nav_link(NavLink.HOME, index)
        
        def _xref(self):
            for (refpage, link) in self.links:
                if link.type != Link.INTERNAL:
                    continue
                
                page = self.find_page(link)
                
                if page is None:
                    link.type = Link.INVALID
                    print >> sys.stderr, 'WARNING: Not found page for link "%s"' % link
                    continue
                
                if not page.header and link.text:
                    page.header = link.text
                elif page.header and not link.text:
                    link.text = page.header
                
                where = link.where
                if '#' in where:
                    anchor = where[link.where.rfind('#'):]
                else:
                    anchor = ''
                
                link.type = Link.EXTERNAL
                link.where = refpage.gen_link_to(page) + anchor
                
        
        def _create_reference(self):
            reference = IndexPage.DocSpaceReference(self.docspace,
                                                    self.references)
            
            self.pages['reference'] = reference
            self.reference = reference
            
            self.links.extend(reference.links)
            
            # Generate index entry for reference
            hobj = Header(IndexPage.REFERENCE_HEADER, IndexPage.CHAPTER_HSIZE)
            list = ListBlock()
            
            ds_ref_link = '%s/%s' % (self.docspace, 'reference')
            link = Link(IndexPage.REFERENCE_LINK_TEXT, Link.INTERNAL, ds_ref_link)
            list.add(ListEntry(1, [link]))
            
            self.blocks.append(Paragraph([hobj, list]))
            self.links.append((self, link)) 
    
    def __init__(self, page_path, doc_header, pages): 
        MarkdownPage.__init__(self, page_path)
        self.header = doc_header
        self.docspace = ''
        
        self.pages = pages
        
        self._process_index()
        
    def generate(self, printer, doc_dir, doc_suffix):
        self.doc_dir = doc_dir
        self.doc_suffix = doc_suffix
        
        self.create_doc_path(doc_dir, doc_suffix)
        
        for docspace in self.docspaces:
            docspace.prepare(self)
        
        for docspace in self.docspaces:
            docspace.generate(printer, self)     
            
            if docspace.is_external:
                # Create link to it - if it was earlier removed 
                hobj = Header(docspace.header, IndexPage.DOCSPACE_HSIZE)
                list = ListBlock()
                
                ds_index_link = '%s/%s%s' % (docspace.docspace,
                                             docspace.name,
                                             doc_suffix) 
                list.add(ListEntry(1, [Link(IndexPage.INDEX_LINK_TEXT, 
                                            Link.EXTERNAL, ds_index_link)]))
                
                self.blocks.append(Paragraph([hobj, list]))
        
        # Generate index itself
        print 'Generating index...'
        
        self.prep_print()
        
        stream = file(self.doc_path, 'w')
        printer.do_print(stream, self.header, self)
    
    def _process_index(self):
        # 1st pass - split index page into smaller indexes
        # one index per document space
        self.docspaces = []
        
        docspace_index = None
        tags = []
        header = None
        docspace = None
        
        is_external = False
        gen_reference = False
        
        blocks = []
        
        for block in self.blocks[:]:
            for part in block.parts[:]:
                if isinstance(part, Header) and \
                        part.size == IndexPage.DOCSPACE_HSIZE:
                    # Found new docspace block
                    # NOTE: Header & reference tags should be within same block
                    if docspace_index is not None:
                        self.docspaces.append(docspace_index)
                    
                    header = part.text
                    docspace_index = None
                    blocks = [block]
                    docspace = None
                    tags = []
                    
                    is_external = False
                    gen_reference = False
                elif isinstance(part, Reference):
                    tags.append(part.text)
                    block.parts.remove(part)
            
            if docspace_index is None and header is not None:
                # Process tag directives
                for tag in tags:
                    if tag == '__external_index__':
                        is_external = True
                    elif tag == '__reference__':
                        gen_reference = True
                    elif tag.startswith('__docspace__'):
                        _, docspace = tag.split(':')
                    
                if not docspace:
                    print >> sys.stderr, 'WARNING: Not found __docspace__ directive for "%s"' % header
                
                if docspace not in self.pages:
                    print >> sys.stderr, 'WARNING: Not found docspace "%s"' % docspace
                    header = None
                    continue
                
                # Create new index entry
                docspace_index = IndexPage.DocSpaceIndex(header, docspace, 
                                                   is_external, gen_reference)
                docspace_index.blocks = blocks
                
                header = None
            else:
                docspace_index.blocks.append(block)
            
            if is_external:
                self.blocks.remove(block)
        
        if docspace_index is not None:
            self.docspaces.append(docspace_index)
        
        # 2nd pass - cross reference pages and links
        for docspace_index in self.docspaces:
            docspace_name = docspace_index.docspace
            docspace_index.process(self.pages[docspace_name], self.docspaces)