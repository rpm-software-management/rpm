## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

all:

noinst_SCRIPTS = initdb showrc querytags ba ckL ckH ckS ckC i qi e

TESTS_ENVIRONMENT = \
  rpm="${top_builddir}/rpm --rcfile ${top_builddir}/tests/$(pkglibdir)/rpmrc" \
  rpm2cpio="${top_builddir}/rpm2cpio" \
  rpmlead="${top_builddir}/tools/rpmlead" \
  rpmheader="${top_builddir}/tools/rpmheader" \
  rpmsignature="${top_builddir}/tools/rpmsignature" \
  rpmdump="${top_builddir}/tools/dump" \
  rpmdumpdb="${top_builddir}/tools/dumpdb" \
  myrpm='../usr/src/redhat/RPMS/*/hello-1.0-1.*.rpm'

TESTS = initdb showrc querytags ba ckL ckH ckS ckC i qi e
