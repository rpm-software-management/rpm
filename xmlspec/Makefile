G++       = g++
CPPFLAGS  = -O2 -Wall#-D_DEBUG_
PREFIX    = /usr/local
DESTDIR   =
LDFLAGS   =

CPPSRCS = XMLAttrs.cpp XMLChangelog.cpp XMLFiles.cpp XMLMacro.cpp \
          XMLMirror.cpp XMLParser.cpp XMLPackage.cpp XMLRequires.cpp \
          XMLScript.cpp XMLSource.cpp XMLSpec.cpp xml2spec.cpp
CPPOBJS = $(CPPSRCS:.cpp=.o)
INCS    = -I. -I.. -I../build -I../lib -I../rpmio
LIBDIR  = #-L../build/.libs
LIBS    = -lexpat -lrpm -lrpmbuild -lrpmdb -lrpmio -lpopt

all: xml2spec

.SUFFIXES: .cpp .o

.cpp.o:
	$(G++) $(CPPFLAGS) $(INCS) -c $<

xml2spec: $(CPPOBJS)
	$(G++) $(CPPFLAGS) $(LDFLAGS) -o xml2spec $(CPPOBJS) $(LIBDIR) $(LIBS)

test: xml2spec
	@./xml2spec example.spec.xml example.out.spec

clean: clean-xml2spec

clean-xml2spec:
	@rm -rf xml2spec *.o
	@rm -rf example.out.spec
	@rm -rf example.out.xml

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp xml2spec $(DESTDIR)$(PREFIX)/bin
