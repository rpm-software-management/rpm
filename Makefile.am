# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = CHANGES CREDITS README.amiga RPM-PGP-KEY \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	docs/[a-z]* gendiff groups $(man_MANS) po/*.in po/*.po po/rpm.pot \
	rpm.magic rpmpopt rpm.spec rpm.8ru rpm2cpio.8ru

SUBDIRS = popt build lib po intl misc tools scripts tests .

INCLUDES = -I$(top_srcdir)/build -I$(top_srcdir)/lib -I$(top_srcdir)/intl @INCPATH@

mylibs=	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/popt/libpopt.la

LDFLAGS = @LDFLAGS_STATIC@

rpmbindir = $(subst usr/bin,bin,$(bindir))
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio
bin_SCRIPTS =		gendiff
noinst_PROGRAMS =	rpmconvert

man_MANS = rpm.8 rpm2cpio.8	# rpm.8ru rpm2cpio.8ru

pkglibdir = @RPMCONFIGDIR@
pkglib_DATA = rpmrc rpmpopt macros
pkglib_SCRIPTS = find-provides find-requires mkinstalldirs \
	config.guess config.sub

noinst_HEADERS = \
	acconfig.h	build.h		checksig.h	ftp.h	\
	install.h	system.h	url.h	\
	verify.h

rpm_SOURCES =		build.c checksig.c ftp.c install.c rpm.c url.c \
			verify.c
rpm_LDADD =		$(mylibs) @LIBMISC@

$(PROGRAMS): 		$(mylibs) @LIBMISC@

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${rpm_SOURCES}

CVSTAG = r$(subst .,-,$(VERSION))

rpm2cpio_SOURCES =	rpm2cpio.c
rpm2cpio_LDADD =	$(mylibs)

rpmconvert_SOURCES =	convertdb.c oldrpmdb.c
rpmconvert_LDADD =	$(mylibs) -lgdbm

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
	@cvs -d `cat CVS/Root` diff >& /dev/null || { \
	    echo " " ; \
	    echo "==> Please resolve differences between the repository and" ; \
	    echo "==> your rpm check out before tagging." ; \
	    echo " " ; \
	    cvs -n up ; \
	    echo " " ; \
	    exit 1 ; \
	}
	@cvs -d `cat CVS/Root` -Q tag -F $(CVSTAG) .
	@make dist
	@echo " "
	@echo "The final archive is ./rpm-$(VERSION).tar.gz."

.PHONY:	nextsrpm
nextsrpm: ./rpm archive
	@sudo ./rpm -ta rpm-$(VERSION).tar.gz
