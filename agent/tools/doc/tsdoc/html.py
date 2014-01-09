head = '''
<html>
<head>
<meta http-equiv="Content-Type" content="text/html">
<meta name="generator" content="tsdoc 0.2">
<title>%s</title>
<link rel="stylesheet" href="../bootstrap/css/bootstrap.min.css" />
<link href="../bootstrap/css/bootstrap-responsive.min.css" rel="stylesheet" />
</head>
<body>

<!-- HEADER -->

<div class="container max-height no-overflow">
    <div id="content" nevow:render="content">
'''

tail = '''
    </div>
</div>

<!-- TAIL -->

</body>
</html>
'''

def html_filter(s):
    s = s.replace('<', '&lt;')
    s = s.replace('>', '&gt;')
    
    return s

class HTMLPrinter:
    def __init__(self, stream):
        self.stream = stream
    
    def prepare(self, title):
        self.stream.write(head % title)
    
    def finalize(self):
        self.stream.write(tail)
    
    def text(self, text):
        self.stream.write(text)
    
    def header(self, size, header):
        size = min(size, 6)
        header = html_filter(header)
        
        self.text('\n<h%d>' % size + header + '</h%d>\n' % size)
    
    def bold_text(self, text):
        self.stream.write('<b>' + html_filter(text) + '</b>')
    
    def italic_text(self, text):
        self.stream.write('<i>' + html_filter(text) + '</i>')
        
    def raw_text(self, text):
        self.stream.write(html_filter(text))
        
    def label_text(self, text):
        self.stream.write('<span class="label label-info">')
        self.stream.write(html_filter(text))
        self.stream.write('</span>&nbsp;')
        
    def list_entry(self):
        self.stream.write('<li>')
    
    def new_line(self, count = 1):
        self.stream.write('<br />\n' * count)
    
    def code_block(self, code):
        self.stream.write('<pre>\n')
        self.stream.write(html_filter(code))
        self.stream.write('</pre>\n')
        
    def paragraph_begin(self):
        self.stream.write('<p>\n')
        
    def paragraph_end(self):
        self.stream.write('</p>\n')