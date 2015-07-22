from tsdoc.blocks import *

class GroffPrinter(Printer):
    single_doc = False
    
    def __init__(self):
        pass    
    
    def do_print(self, stream, header, page):
        self.stream = stream
        for block in page:
            self._print_block(block)
    
    def _strip_text(self, text):
        lines = (l.strip() for l in text.split('\n'))
        # No need for line breaks in paragraphs, roff will take care of it
        return ' '.join(lines)
    
    def _print_block(self, block):
        if isinstance(block, Table):
            return self._print_table(block)
        
        if isinstance(block, ListBlock):
            self.stream.write('.RS\n')
        
        if isinstance(block, BlockQuote):
            self.stream.write('.QP\n')
        elif isinstance(block, Code):
            self.stream.write('.sp\n.in +4n\n.nf\n')
        elif isinstance(block, ListEntry):
            self.stream.write('.IP * 2\n')
        elif isinstance(block, Paragraph):
            self.stream.write('.PP\n')
        
        for part in block:
            if isinstance(part, Block):
                self._print_block(part)
            else:
                if not isinstance(block, Code):
                    text = self._strip_text(str(part))
                else:
                    text = str(part)
                
                if isinstance(part, Header):
                    self.stream.write('.SH ')
                    text = '"' + text.upper() + '"'
                elif isinstance(part, ItalicText):
                    self.stream.write('.I ')
                elif isinstance(part, BoldText):
                    self.stream.write('.B ')
                
                elif isinstance(part, InlineCode):
                    self.stream.write('.SM ')
                
                self.stream.write(text)
                self.stream.write('\n')
        
        if isinstance(block, ListBlock):
            self.stream.write('.RE\n')
        elif isinstance(block, Code):
           self.stream.write('.fi\n.in\n.pp')
        
        
    def _print_table(self, table):
        rowspan = []
        
        self.stream.write('.TS\n')
        self.stream.write('tab(|) box;\n')
        
        rowno = 0
        
        for row in table:
            if rowno == 0:
                # Use first row to fill table format specification
                rowlen = sum(cell.rowspan for cell in row)
                self.stream.write('|'.join(['l'] * rowlen) + '.\n')
            else:
                self.stream.write('\n_\n')
            
            colno = 0
            if not isinstance(row, TableRow):
                continue
            
            for cell in row:
                if not isinstance(cell, TableCell):
                    continue
                
                colno = self._print_table_cell(cell, rowspan, colno)
            
            rowno += 1
                
        self.stream.write('\n.TE\n.PP\n')
    
    
    def _print_table_cell(self, cell, rowspan, colno):
        def print_delim():
            if colno >= 1:
                self.stream.write('|')
        
        # Fill rowspan gaps
        while colno >= len(rowspan):
            rowspan.append(0)
        while rowspan[colno] != 0 and colno < len(rowspan):
            rowspan[colno] -= 1
            
            print_delim()
            self.stream.write('v')
            
            colno += 1
        
        rowspan[colno] = cell.rowspan - 1
        
        print_delim()
        self._print_block_raw(cell)
        
        # Add colspan
        for i in range(cell.colspan):
            colno += 1
            if i > 0:
                print_delim()
                self.stream.write('>')
        
        return colno
    
    def _print_block_raw(self, block):
        for part in block:
            if isinstance(part, Block):
                self._print_block_raw(part)
            else:
                text = self._strip_text(str(part))
                self.stream.write(text)
    
                