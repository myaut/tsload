def md_filter(s):
    s = s.replace('_', '\\_')
    s = s.replace('*', '\\*')
    
    return s

class MarkdownPrinter:
    def __init__(self, stream):
        self.stream = stream
    
    def text(self, text):
        self.stream.write(text)
    
    def header(self, size, header):
        size = min(size, 6)
        header = md_filter(header)
        
        self.text('\n' + '#' * size + ' ' + header + '\n')
    
    def bold_text(self, text):
        self.stream.write('**' + md_filter(text) + '**')
    
    def italic_text(self, text):
        self.stream.write('_' + md_filter(text) + '_')
        
    def raw_text(self, text):
        self.stream.write(md_filter(text))
        
    def list_entry(self):
        self.stream.write(' * ')
    
    def new_line(self, count = 1):
        self.stream.write('\n' * count)
    
    def code_block(self, code):
        self.stream.write('\n```\n')
        self.stream.write(code)
        self.stream.write('\n```\n')