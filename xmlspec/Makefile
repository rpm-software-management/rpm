G++        = g++
CPPFLAGS   = -O2 -Wall#-D_DEBUG_
PREFIX     = /usr/local
DESTDIR    =
LDFLAGS    =

CPPSRCS    = XMLAttrs.cpp XMLChangelog.cpp XMLFiles.cpp XMLMacro.cpp \
             XMLMisc.cpp XMLMirror.cpp XMLParser.cpp XMLPackage.cpp \
             XMLRequires.cpp XMLRPMWrap.cpp XMLScript.cpp XMLSource.cpp \
             XMLSpec.cpp XMLText.cpp
TOSPECSRC  = $(CPPSRCS) xml2spec.cpp
TOXMLSRC   = $(CPPSRCS) spec2xml.cpp

CPPOBJS    = $(CPPSRCS:.cpp=.o)
TOSPECOBJS = $(TOSPECSRC:.cpp=.o)
TOXMLOBJS  = $(TOXMLSRC:.cpp=.o)

INCS       = -I. -I.. -I../build -I../lib -I../popt -I../rpmio
LIBDIR     = #-L../build/.libs
LIBS       = -lexpat -lrpm -lrpmbuild -lrpmdb -lrpmio -lpopt

all: xml2spec spec2xml

.SUFFIXES: .cpp .o

.cpp.o:
	$(G++) $(CPPFLAGS) $(INCS) -c $<

xml2spec: $(TOSPECOBJS)
	$(G++) $(CPPFLAGS) $(LDFLAGS) -o xml2spec $(TOSPECOBJS) $(LIBDIR) $(LIBS)

spec2xml: $(TOXMLOBJS)
	$(G++) $(CPPFLAGS) $(LDFLAGS) -o spec2xml $(TOXMLOBJS) $(LIBDIR) $(LIBS)

test: xml2spec spec2xml
	@./xml2spec example.spec.xml example.out.spec
	@./spec2xml example.out.spec example.out.xml

clean:
	@rm -rf xml2spec spec2xml *.o
	@rm -rf example.out.spec
	@rm -rf example.out.xml

install:
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp xml2spec spec2xml $(DESTDIR)$(PREFIX)/bin
