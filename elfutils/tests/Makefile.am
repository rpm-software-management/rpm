## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 1996-2001, 2002 Red Hat, Inc.
##
## This program is Open Source software; you can redistribute it and/or
## modify it under the terms of the Open Software License version 1.0 as
## published by the Open Source Initiative.
##
## You should have received a copy of the Open Software License along
## with this program; if not, you may obtain a copy of the Open Software
## License version 1.0 from http://www.opensource.org/licenses/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.
##
DEFS = -DHAVE_CONFIG_H -D_GNU_SOURCE
AM_CFLAGS = -Wall # -Werror
INCLUDES = -I$(top_srcdir)/libasm -I$(top_srcdir)/libdwarf \
	   -I$(top_srcdir)/libdw \
	   -I$(top_srcdir)/libebl -I$(top_srcdir)/libelf \
	   -I$(top_srcdir)/lib -I..

noinst_PROGRAMS = arextract arsymtest newfile saridx scnnames sectiondump \
		  showptable update1 update2 update3 update4 test-nlist \
		  get-cus firstdie show-dies show-tags show-die-info1 \
		  show-die-info2 get-lines get-files get-pubnames get-aranges \
		  show-ciefde show-abbrev hash asm-tst1 asm-tst2 asm-tst3 \
		  asm-tst4 asm-tst5 asm-tst6 asm-tst7 asm-tst8 asm-tst9 \
		  msg_tst newscn ecp \
		  get-pubnames2

TESTS = run-arextract.sh run-arsymtest.sh newfile test-nlist \
	update1 update2 update3 update4 run-get-cus.sh \
	run-firstdie.sh run-show-dies.sh run-show-tags.sh \
	run-show-die-info1.sh run-show-die-info2.sh run-get-files.sh \
	run-get-lines.sh run-get-pubnames.sh run-get-aranges.sh \
	run-show-ciefde.sh run-show-abbrev.sh hash asm-tst1 asm-tst2 \
	asm-tst3 asm-tst4 asm-tst5 asm-tst6 asm-tst7 asm-tst8 asm-tst9 \
	msg_tst newscn run-strip-test.sh run-strip-test2.sh \
	run-get-pubnames2.sh run-ecp-test.sh run-ecp-test2.sh

EXTRA_DIST = run-arextract.sh run-arsymtest.sh run-firstdie.sh \
	     run-show-dies.sh run-show-tags.sh run-show-die-info1.sh \
	     run-show-die-info2.sh run-get-files.sh run-get-lines.sh \
	     run-get-pubnames.sh run-get-aranges.sh run-get-cus.sh \
	     run-show-ciefde.sh run-show-abbrev.sh run-strip-test.sh \
	     run-strip-test2.sh run-ecp-test.sh run-ecp-test2.sh \
	     run-get-pubnames2.sh \
	     testfile testfile2 testfile3 testfile4 testfile5 \
	     testfile6.bz2 testfile7.bz2 testfile8.bz2 testfile9.bz2 \
	     testfile10

TESTS_ENVIRONMENT = LD_LIBRARY_PATH="../libelf:../libebl:../libasm:../libdwarf:../libdw$${LD_LIBRARY_PATH:+:}$$LD_LIBRARY_PATH"

arextract_LDADD = ../libelf/libelf.so
arsymtest_LDADD = ../libelf/libelf.so
newfile_LDADD = ../libelf/libelf.so
saridx_LDADD = ../libelf/libelf.so
scnnames_LDADD = ../libelf/libelf.so
sectiondump_LDADD = ../libelf/libelf.so
showptable_LDADD = ../libelf/libelf.so
update1_LDADD = ../libelf/libelf.so
update2_LDADD = ../libelf/libelf.so
update3_LDADD = ../libebl/libebl.so ../libelf/libelf.so
update4_LDADD = ../libebl/libebl.so ../libelf/libelf.so
test_nlist_LDADD = ../libelf/libelf.so
get_cus_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
firstdie_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_dies_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_tags_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_die_info1_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_die_info2_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
get_lines_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
get_files_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
get_pubnames_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
get_aranges_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_ciefde_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
show_abbrev_LDADD = ../libdwarf/libdwarf.so ../libelf/libelf.so
hash_LDADD = ../libelf/libelf.so
asm_tst1_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst2_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst3_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst4_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst5_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst6_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst7_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst8_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
asm_tst9_LDADD = ../libasm/libasm.so ../libebl/libebl.so ../libelf/libelf.so
msg_tst_LDADD = ../libelf/libelf.so
newscn_LDADD = ../libelf/libelf.so
ecp_LDADD = ../libelf/libelf.so
get_pubnames2_LDADD = ../libdw/libdw.so ../libelf/libelf.so

CLEANFILES = xxx
