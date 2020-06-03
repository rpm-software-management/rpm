## Process this file with automake to produce Makefile.in

EXTRA_DIST =
EXTRA_DIST += index.md

man_man1dir = $(mandir)/man1
man_man1_DATA = gendiff.1
EXTRA_DIST += gendiff.1.md

man_man8dir = $(mandir)/man8
man_man8_DATA = rpm.8 rpm-misc.8 rpmbuild.8 rpmdeps.8 rpmgraph.8 rpm2cpio.8
man_man8_DATA += rpmdb.8 rpmkeys.8 rpmsign.8 rpmspec.8

EXTRA_DIST += rpm.8.md rpm-misc.8.md rpmbuild.8.md rpmdeps.8.md rpmgraph.8.md
EXTRA_DIST += rpm2cpio.8.md rpmdb.8.md rpmkeys.8.md rpmsign.8.md rpmspec.8.md

# Package generated man pages
EXTRA_DIST += $(man_man1_DATA)
EXTRA_DIST += $(man_man8_DATA)

if ENABLE_PLUGINS
man_man8_DATA += rpm-plugin-prioreset.8 rpm-plugin-syslog.8
man_man8_DATA += rpm-plugins.8

if AUDIT
man_man8_DATA += rpm-plugin-audit.8
endif
if DBUS
man_man8_DATA += rpm-plugin-systemd-inhibit.8
man_man8_DATA += rpm-plugin-dbus-announce.8
endif
if IMA
man_man8_DATA += rpm-plugin-ima.8
endif
if FAPOLICYD
man_man8_DATA += rpm-plugin-fapolicyd.8
endif
if SELINUX
man_man8_DATA += rpm-plugin-selinux.8
endif
endif

if WITH_ARCHIVE
man_man8_DATA += rpm2archive.8
endif

EXTRA_DIST += rpm-plugins.8.md rpm-plugin-prioreset.8.md rpm-plugin-syslog.8.md
EXTRA_DIST += rpm-plugin-audit.8.md rpm-plugin-systemd-inhibit.8.md
EXTRA_DIST += rpm-plugin-ima.8.md rpm-plugin-selinux.8.md rpm2archive.8.md
EXTRA_DIST += rpm-plugin-fapolicyd.8.md rpm-plugin-dbus-announce.8.md

# Package generated man pages
EXTRA_DIST += rpm-plugins.8 rpm-plugin-prioreset.8 rpm-plugin-syslog.8
EXTRA_DIST += rpm-plugin-audit.8 rpm-plugin-systemd-inhibit.8
EXTRA_DIST += rpm-plugin-ima.8 rpm-plugin-selinux.8 rpm2archive.8
EXTRA_DIST += rpm-plugin-fapolicyd.8


man_fr_man8dir = $(mandir)/fr/man8
man_fr_man8_DATA = fr/rpm.8
EXTRA_DIST += fr/rpm.8.md
EXTRA_DIST += fr/rpm.8

man_ja_man8dir = $(mandir)/ja/man8
man_ja_man8_DATA = ja/rpm.8 ja/rpm2cpio.8 ja/rpmbuild.8 ja/rpmgraph.8
EXTRA_DIST += ja/rpm.8.md ja/rpm2cpio.8.md ja/rpmbuild.8.md ja/rpmgraph.8.md
EXTRA_DIST += ja/rpm.8 ja/rpm2cpio.8 ja/rpmbuild.8 ja/rpmgraph.8

man_ko_man8dir = $(mandir)/ko/man8
man_ko_man8_DATA = ko/rpm.8 ko/rpm2cpio.8
EXTRA_DIST += ko/rpm.8.md ko/rpm2cpio.8.md
EXTRA_DIST += ko/rpm.8 ko/rpm2cpio.8

man_pl_man1dir = $(mandir)/pl/man1
man_pl_man1_DATA = pl/gendiff.1
man_pl_man8dir = $(mandir)/pl/man8
man_pl_man8_DATA = pl/rpm.8 pl/rpm2cpio.8 pl/rpmbuild.8 pl/rpmdeps.8 pl/rpmgraph.8
EXTRA_DIST += pl/gendiff.1.md pl/rpm.8.md pl/rpm2cpio.8.md pl/rpmbuild.8.md pl/rpmdeps.8.md pl/rpmgraph.8.md
EXTRA_DIST += pl/gendiff.1 pl/rpm.8 pl/rpm2cpio.8 pl/rpmbuild.8 pl/rpmdeps.8 pl/rpmgraph.8

man_ru_man8dir = $(mandir)/ru/man8
man_ru_man8_DATA = ru/rpm.8 ru/rpm2cpio.8
EXTRA_DIST += ru/rpm.8.md ru/rpm2cpio.8.md
EXTRA_DIST += ru/rpm.8 ru/rpm2cpio.8

man_sk_man8dir	= $(mandir)/sk/man8
man_sk_man8_DATA = sk/rpm.8
EXTRA_DIST += sk/rpm.8.md
EXTRA_DIST += sk/rpm.8

%: $(srcdir)/%.md
	@$(MKDIR_P) `dirname $@`
	${PANDOC} -s -t man $(srcdir)/$@.md -o $@

clean-local:
	rm -f *.8 *.1 */*.8 */*.1
