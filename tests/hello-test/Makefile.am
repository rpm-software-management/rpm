## Process this file with automake to produce Makefile.in.

all:

noinst_SCRIPTS = initdb showrc ba ckL ckH ckS ckC

TESTS_ENVIRONMENT = \
  rpm="${top_builddir}/rpm --rcfile ${top_builddir}/tests/$(pkglibdir)/rpmrc" \
  rpm2cpio="${top_builddir}/rpm2cpio" \
  rpmlead="${top_builddir}/tools/rpmlead" \
  rpmheader="${top_builddir}/tools/rpmheader" \
  rpmsignature="${top_builddir}/tools/rpmsignature" \
  rpmdump="${top_builddir}/tools/dump" \
  myrpm="../usr/src/redhat/RPMS/sparc/hello-1.0-1.sparc.rpm"

TESTS = initdb showrc ba ckL ckH ckS ckC
