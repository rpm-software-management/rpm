import sys, libxml2

class RpmLibxml2Parser:
    def __init__(self, fn):
        try:
	    self.p = libxml2.newTextReaderFilename(fn)
        except:
	    print "unable to open %s" % (fn)
	    return
	self.p.SetParserProp(libxml2.PARSER_VALIDATE, 1)
	self.p.SetParserProp(libxml2.PARSER_SUBST_ENTITIES, 1)
	self.n = 2
	self.lvl = 0
	self.cdata = 0

    def spew(self, l):
	sys.stdout.write(l)
	sys.stdout.flush()

    def pad(self):
	return (' ' * (self.n * self.lvl))

    def start_element_handler(self, name, attrs):
	l = self.pad() + '<' + name
	if attrs.has_key(u'name'):
	    l = l + ' name="' + attrs[u'name'] + '"'
	l = l + '>'
	if self.lvl < 2:
	    l = l + '\n'
	self.spew(l)
	self.lvl = self.lvl + 1
	self.cdata = 1
    
    def end_element_handler(self, name):
	self.lvl = self.lvl - 1
	l = '</' + name + '>'
	if self.lvl < 2:
	    l = self.pad() + l
	l = l + '\n'
	self.spew(l)
	self.cdata = 0

    def character_data_handler(self, data):
	if self.cdata == 1:
	    if not data.isspace():
		self.cdata = 2
	if self.cdata > 1:
	    self.spew(data)

    def processNode(self):
	if self.p.NodeType() == libxml2.XML_READER_TYPE_ELEMENT:	# 1
	    name = self.p.Name()
	    attrs = {}
	    while self.p.MoveToNextAttribute():
		attrs[self.p.Name()] = self.p.Value()
	    self.start_element_handler(name, attrs)
	elif self.p.NodeType() == libxml2.XML_READER_TYPE_TEXT:		# 3
	    self.character_data_handler(self.p.Value())
	elif self.p.NodeType() == libxml2.XML_READER_TYPE_DOCUMENT_TYPE:# 10
	    self.character_data_handler(self.p.Value())
	elif self.p.NodeType() == libxml2.XML_READER_TYPE_SIGNIFICANT_WHITESPACE:# 14
	    self.character_data_handler(self.p.Value())
	elif self.p.NodeType() == libxml2.XML_READER_TYPE_END_ELEMENT:	# 15
	    self.end_element_handler(self.p.Name())
	else:
	    print "%d %d %s IsEmptyElement:%d %s" % (self.p.Depth(), self.p.NodeType(),
                              self.p.Name(), self.p.IsEmptyElement(),
                              self.p.Value())
	    while self.p.MoveToNextAttribute():
		print "-- %d %d (%s) [%s]" % (self.p.Depth(), self.p.NodeType(),
                                          self.p.Name(), self.p.Value())

    def read(self, *args):
	return self.p.Read(*args)

    def parseFile(self):
	ret = self.read()
	while ret == 1:
	    self.processNode()
	    ret = self.read()
	return ret

fn = 'time.xml'
p = RpmLibxml2Parser(fn)
ret = p.parseFile()
if ret != 0:
    print "Error parsing and validating %s" % (fn)
