import sys, xml.parsers.expat

class RpmExpatParser:
    def __init__(self, fn):
	self.f = open(fn)
	self.p = xml.parsers.expat.ParserCreate()
	self.p.StartElementHandler = self.start_element
	self.p.EndElementHandler = self.end_element
	self.p.CharacterDataHandler = self.char_data
	self.n = 2
	self.lvl = 0

    def spew(self, l):
	sys.stdout.write(l)
	sys.stdout.flush()

    def pad(self):
	return (' ' * (self.n * self.lvl))

    def start_element(self, name, attrs):
	l = self.pad() + '<' + name
	if attrs.has_key(u'name'):
	    l = l + ' name=' + attrs[u'name']
	l = l + '>'
	if self.lvl < 2:
	    l = l + '\r\n'
	self.spew(l)
	self.lvl = self.lvl + 1
    
    def end_element(self, name):
	self.lvl = self.lvl - 1
	l = '</' + name + '>'
	if self.lvl < 2:
	    l = self.pad() + l
	l = l + '\r\n'
	self.spew(l)

    def char_data(self, data):
	if not data.isspace():
	    self.spew(data)

    def read(self, *args):
	return self.f.read(*args)

    def ParseFile(self):
	self.p.ParseFile(self)

p = RpmExpatParser('time.xml')
p.ParseFile()
