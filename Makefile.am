# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.3 foreign

SUBDIRS = popt build lib po intl misc tools scripts

INCLUDES = -I./build -I./lib -I./intl @INCPATH@

LDFLAGS = @LDFLAGS_STATIC@ -L./popt -L./build -L./lib -L./misc

rpmbindir = $(subst usr/bin,bin,$(bindir))
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio
bin_SCRIPTS =		gendiff
noinst_PROGRAMS =	rpmconvert

man_MANS = rpm.8 rpm2cpio.8

configdir = @RPMCONFIGDIR@
config_DATA = rpmrc rpmpopt macros
config_SCRIPTS = find-provides find-requires mkinstalldirs

noinst_HEADERS = \
	acconfig.h	build.h		checksig.h	ftp.h	\
	install.h	query.h		system.h	url.h	\
	verify.h

rpm_SOURCES =		build.c checksig.c ftp.c install.c query.c rpm.c url.c \
			verify.c
rpm_LDADD =		-lrpmbuild -lpopt -lrpm @LIBMISC@

rpm.o:
	$(COMPILE) \
		-DLOCALEDIR=\"$(LOCALEDIR)\" \
		-DLIBRPMALIAS_FILENAME=\"$(LIBRPMALIAS_FILENAME)\" \
		-DRPMCONFIGDIR="\"$(RPMCONFIGDIR)"\" \
		-c rpm.c

include ./Makefile.inc

CVSTAG = r$(subst .,-,$(VERSION))

rpm2cpio_SOURCES =	rpm2cpio.c
rpm2cpio_LDADD =	-lrpmbuild -lpopt -lrpm

rpmconvert_SOURCES =	convertdb.c oldrpmdb.c
rpmconvert_LDADD =	-lrpmbuild -lpopt -lrpm -lgdbm

install-data-local:
	@rm -f $(libdir)/rpmrc	&& ln -s rpm/rpmrc $(DESTDIR)/$(libdir)/rpmrc
	@rm -f $(libdir)/rpmpopt && ln -s rpm/rpmpopt $(DESTDIR)/$(libdir)/rpmpopt

# FIXME: these should be generated in configure.in
find-requires: find-requires.sh
	cp $< $@
find-provides: find-provides.sh
	cp $< $@
rpmrc: lib-rpmrc
	cp $< $@

# FIXME: these should be checked
.PHONY:                tar
tar:
	rm -rf /tmp/rpm-$(VERSION)
	make installprefix=/tmp/rpm-$(VERSION)$(ROOT) install
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/SOURCES
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/SPECS
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/SRPMS
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/RPMS
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/BUILD
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/var/local/lib/rpm
	$(mkinstalldirs) /tmp/rpm-$(VERSION)/var/tmp
#	Couldn't figure de ARCH value   
#	$(mkinstalldirs) /tmp/rpm-$(VERSION)/usr/local/src/redhat/RPMS/i386
	cd /tmp/rpm-$(VERSION) ; tar cvf /tmp/rpm-$(VERSION).tar .

.PHONY: noconfig
noconfig:
	find . -name "Makefile" -exec rm {} \; 
	rm -f *gz *rpm config.*

.PHONY: archive
archive: 
	@echo " "
	@echo "I hope you checked everything out and made sure it builds"
	@echo "maybe someday Erik will get around to making that automatic."
	@echo
	@echo "This is version $(VERSION)."
	@sleep 5
	@cvs -Q tag -F $(CVSTAG) .
	@rm -rf /tmp/rpm-$(VERSION) /tmp/rpm
	@cd /tmp; cvs -Q -d $(CVSROOT) export -r$(CVSTAG) rpm || :
	@mv /tmp/rpm /tmp/rpm-$(VERSION)
	@rm /tmp/rpm-$(VERSION)/popt/popt.spec
	@cd /tmp/rpm-$(VERSION); ./autogen.sh ; make depend; make distclean
	@cd /tmp/rpm-$(VERSION); ./autogen.sh --noconfigure
	@cd /tmp; tar czSpf rpm-$(VERSION).tar.gz rpm-$(VERSION)
	@rm -rf /tmp/rpm-$(VERSION)
	@cp /tmp/rpm-$(VERSION).tar.gz .
	@rm -f /tmp/rpm-$(VERSION).tar.gz 
	@echo " "
	@echo "The final archive is ./rpm-$(VERSION).tar.gz."
