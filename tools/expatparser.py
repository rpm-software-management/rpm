import sys, xml.parsers.expat

class RpmExpatParser:
    def __init__(self, fn):
	try:
	    self.f = open(fn)
	except:
	    print "unable to open %s" % (fn)
	    return
	self.p = xml.parsers.expat.ParserCreate()
	self.p.StartElementHandler = self.start_element_handler
	self.p.EndElementHandler = self.end_element_handler
	self.p.CharacterDataHandler = self.character_data_handler
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

    def read(self, *args):
	return self.f.read(*args)

    def parseFile(self):
	return self.p.ParseFile(self)

p = RpmExpatParser('time.xml')
ret = p.parseFile()
if ret != 1:
    print "Error parsing and validating %s" % (fn)
