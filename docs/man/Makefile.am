## Process this file with automake to produce Makefile.in

EXTRA_DIST =

man_man1dir = $(mandir)/man1
man_man1_DATA = gendiff.1
EXTRA_DIST += $(man_man1_DATA)

man_man8dir = $(mandir)/man8
man_man8_DATA = rpm.8 rpm-misc.8 rpmbuild.8 rpmdeps.8 rpmgraph.8 rpm2cpio.8
man_man8_DATA += rpmdb.8 rpmkeys.8 rpmsign.8 rpmspec.8

EXTRA_DIST += $(man_man8_DATA)

if ENABLE_PLUGINS
man_man8_DATA += rpm-plugin-prioreset.8 rpm-plugin-syslog.8
man_man8_DATA += rpm-plugins.8

if AUDIT
man_man8_DATA += rpm-plugin-audit.8
endif
if DBUS
man_man8_DATA += rpm-plugin-systemd-inhibit.8
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

EXTRA_DIST += rpm-plugins.8 rpm-plugin-prioreset.8 rpm-plugin-syslog.8 
EXTRA_DIST += rpm-plugin-audit.8 rpm-plugin-systemd-inhibit.8 
EXTRA_DIST += rpm-plugin-ima.8 rpm-plugin-selinux.8 rpm2archive.8
EXTRA_DIST += rpm-plugin-fapolicyd.8


man_fr_man8dir = $(mandir)/fr/man8
man_fr_man8_DATA = fr/rpm.8
EXTRA_DIST += $(man_fr_man8_DATA)

man_ja_man8dir = $(mandir)/ja/man8
man_ja_man8_DATA = ja/rpm.8 ja/rpm2cpio.8 ja/rpmbuild.8 ja/rpmgraph.8
EXTRA_DIST += $(man_ja_man8_DATA)

man_ko_man8dir = $(mandir)/ko/man8
man_ko_man8_DATA = ko/rpm.8 ko/rpm2cpio.8
EXTRA_DIST += $(man_ko_man8_DATA)

man_pl_man1dir = $(mandir)/pl/man1
man_pl_man1_DATA = pl/gendiff.1
man_pl_man8dir = $(mandir)/pl/man8
man_pl_man8_DATA = pl/rpm.8 pl/rpm2cpio.8 pl/rpmbuild.8 pl/rpmdeps.8 pl/rpmgraph.8
EXTRA_DIST += $(man_pl_man1_DATA) $(man_pl_man8_DATA)

man_ru_man8dir = $(mandir)/ru/man8
man_ru_man8_DATA = ru/rpm.8 ru/rpm2cpio.8
EXTRA_DIST += $(man_ru_man8_DATA)

man_sk_man8dir	= $(mandir)/sk/man8
man_sk_man8_DATA = sk/rpm.8
EXTRA_DIST += $(man_sk_man8_DATA)
