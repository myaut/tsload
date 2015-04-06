## TSDoc

TSDoc is an internal TSLoad engine for generating documentation which supports here-docs inside C sources in Markdown and standalone Markdown text pages and converts them to a desired output format: HTML, LaTeX or Text. TSDoc consists of several components which are located in `tools/doc`:
 * __Source parser__. Originally, DoxyGen was used to parse sources, but since it hard to integrate with other docs and DoxyGen doesn't respect various TSLoad codestyle features, it has own parser. Sources are preprocessed into `.tsdoc` file that is later used by builder.
 * __Markdown parser__. Parses incoming markdown text and represents it as tree of blocks -- objects of `tsdoc.blocks.Block` classs and similiar objects.
 * __Printer__. Prints page representation as a list of Blocks in desired format
 * __Builder__. Collects pages, resolves references between them and builds destination files using one of printers. Also generates special pages such as index and reference pages.
 
### Sources 

TSDoc generates API documentation, thus only declarations with here-docs or functions that have `LIBEXPORT` qualifier are presented in documentation. TSLoad also documents types that are used in a function parameters. There are two special qualifiers that alter this behavior: `TSDOC_FORCE` enforces generation while `TSDOC_HIDDEN` disables it. 

Multiple files may provide documentation for one page: main header file (should be parsed first) and multiple source and header files. If some function doesn't have here-doc in main header file, it can be taken from source file, etc. Definitions in main header file are grouped. Double newline delimits groups. If group have common here-doc than builder will allocate single `Block` to represent that definition, but if there is separate here-doc for one of the elements of that group, it will be separated. Each element has its own name (variable name, function name, type name) so group will have header: comma-separated list of that names (unless `@name` directive isn't used). 

Like DoxyGen, TSLoad here-doc starts with `/**`, ends with `*/` and should precede documentation declaration (no additional newlines allowed). Other here-doc forms like `//<` are not allowed. Here-docs that doesn't correspond to any of declarations will be considered "module" here-docs and will be added to the beginning of the documentation page. TSDoc also cuts out first asterisk `*` at the beginning of each line of here-doc. 

TSDoc here-docs support following directives:

---
__Directive__ | __Description__
2,1 Any of markdown directives 
@module | Name of module. Will be used as header in page and as a name of links referring this module
@name | Name of declaration group
@param ARG | For function declarations: name of argument
@member FIELD | For complex type declarations: name of field
@value VALUE | For enumerations or constants: value
@note | Note 
@see | Reference (not actually parsed as reference)
@return | Note about returning value
---

Directives are ending with newline, but can be expanded with backslash `\`. They are followed by text corresponding to that directive (it also supports Markdown, but  no blocks such as code block, headers and tables). 

### Markdown

Markdown is used in TSLoad documentation, thus there are some extensions to it:

---
__Markdown directive__ | __Description__ 
2,1 __Embedded directives__
`_text_` or `*text*` | Italic text
`__text__` or `**text**` | __Bold text__
`` `code` `` | `Embedded code`
`[tag]` | HTML anchor for references or a tag 
`[link text][docspace/page]` | Intra-documentation link
`[link text](external link)` | External link
`![alt text](link to image)` | Image
` ### Header ` | Header. Number of number signs will be level of header (from 1 to 6).  Header ends with newline.
2,1 __Block directives__
` * List element ` | List element. Indentation of list element will be its depth.
` > Block quote ` | Block quote
` ```  ` | Beginning and ending of code block
2,1 __Tables__
` --- ` | Beginning and ending of table (block)
` | ` | Cell separator inside table
`N,M ` | At the beginning of cell - rowspan and colspan correspondingly.
---

Markdown directives may be escaped with backlash `\`. 

### Builder and printer

Builder is a heart of TSDoc. Builder reads incoming files in one of two formats:
  * `.src.md` -- Markdown,
  * `.tsdoc` -- preprocessed sources.

It parses markdown, and generates additional text for preprocessed sources such as sections and headers, function visibility labels. Builder is also responsible for setting inter-page references and links. Inter-page link consists from two or three parts including in the following format: `docspace/page#reference` where reference part is optional. When documentation is built, builder creates in-memory list of a pages and replaces each inter-page link with a corresponding format link, i.e with `<a href=''>` in html, or if that link cannot be found, it marks it with red font. If link text is omitted, TSLoad may try to pick it from most-significant header of Markdown page or `@module` directive of a source file.

Documentation is split into "docspaces" - bunches of markdown and sources files that cover similiar topics, like this page is part of development documentation. Markdown pages have following path relative to agent sources root: `doc/docspace/page.src.md`. Main page of a documentation is located in a `doc/` subdirectory and called `index.src.md`. TSDoc firstly processes that file to find docspaces and its properties. Each docspace starts with 3rd level header followed by "docspace tags" -- special form of tag that doesn't parsed into a block but contains special settings for that docspace:
 * `[__docspace__:docspace]` (mandatory) -- name of docspace.
 * `[__external_index__]` -- create external index page for that docspace and put link to that file. However to generate index, root `index.src.md` is still used.
 * `[__reference__]` -- generate external reference for API documentation sorted alphabetically. Some libraries like HostInfo have similiar prefix for all their functions and types (`hi_` in that case), so `[__refprefix__:hi_]` is provided meaning that prefix will not be used while grouping entries in reference.

Order of links inside index file is used to generate links to next, previous and upper page. For single-paged formats like TeX, it will also used to order information in output files. Note that index file is also used in builder emitter inside [SCons][devel/build] to determine which files will be generated and correctly install them.
 
After preprocessing all files, Bulder runs Printer to generate output files.