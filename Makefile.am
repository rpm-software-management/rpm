# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.3 foreign

SUBDIRS = popt build lib po intl misc tools scripts tests

INCLUDES = -I$(top_srcdir)/build -I$(top_srcdir)/lib -I$(top_srcdir)/intl @INCPATH@

LDFLAGS = @LDFLAGS_STATIC@ -L$(top_builddir)/build -L$(top_builddir)/lib -L$(top_builddir)/popt -L$(top_builddir)/misc

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
	install.h	system.h	url.h	\
	verify.h

rpm_SOURCES =		build.c checksig.c ftp.c install.c rpm.c url.c \
			verify.c
rpm_LDADD =		-lrpmbuild -lrpm -lpopt @LIBMISC@

$(PROGRAMS): ./build/librpmbuild.a ./lib/librpm.a ./popt/libpopt.a

include ./Makefile.inc

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${rpm_SOURCES}

CVSTAG = r$(subst .,-,$(VERSION))

rpm2cpio_SOURCES =	rpm2cpio.c
rpm2cpio_LDADD =	-lrpmbuild -lrpm -lpopt

rpmconvert_SOURCES =	convertdb.c oldrpmdb.c
rpmconvert_LDADD =	-lrpmbuild -lrpm -lpopt -lgdbm

install-data-local:
	@$(mkinstalldirs) $(DESTDIR)/var/lib/rpm
	@rm -f $(DESTDIR)/$(libdir)/rpmrc
	@ln -s rpm/rpmrc $(DESTDIR)/$(libdir)/rpmrc
	@rm -f $(DESTDIR)/$(libdir)/rpmpopt
	@ln -s rpm/rpmpopt $(DESTDIR)/$(libdir)/rpmpopt
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/BUILD
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/noarch
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/@build_cpu@
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SOURCES
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SPECS
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SRPMS
	@$(mkinstalldirs) $(DESTDIR)/var/lib/rpm
	@$(mkinstalldirs) $(DESTDIR)/var/tmp

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
	@make -C /tmp/rpm-$(VERSION) depend distclean
	@cd /tmp/rpm-$(VERSION); ./autogen.sh --noconfigure
	@cd /tmp; tar czSpf rpm-$(VERSION).tar.gz rpm-$(VERSION)
	@rm -rf /tmp/rpm-$(VERSION)
	@cp /tmp/rpm-$(VERSION).tar.gz .
	@rm -f /tmp/rpm-$(VERSION).tar.gz 
	@echo " "
	@echo "The final archive is ./rpm-$(VERSION).tar.gz."
