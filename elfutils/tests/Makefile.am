## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits
DEFS = -DHAVE_CONFIG_H -D_GNU_SOURCE
AM_CFLAGS = -Wall -Werror
INCLUDES = -I$(top_srcdir)/libasm -I$(top_srcdir)/libdwarf \
	   -I$(top_srcdir)/libebl -I$(top_srcdir)/libelf \
	   -I$(top_srcdir)/lib -I..

noinst_PROGRAMS = arextract arsymtest newfile saridx scnnames sectiondump \
		  showptable update1 update2 update3 update4 test-nlist \
		  get-cus firstdie show-dies show-tags show-die-info1 \
		  show-die-info2 get-lines get-files get-pubnames get-aranges \
		  show-ciefde show-abbrev hash asm-tst1 asm-tst2 asm-tst3 \
		  asm-tst4 asm-tst5 asm-tst6 asm-tst7 asm-tst8 msg_tst

TESTS = run-arextract.sh run-arsymtest.sh newfile test-nlist \
	update1 update2 update3 update4 run-get-cus.sh \
	run-firstdie.sh run-show-dies.sh run-show-tags.sh \
	run-show-die-info1.sh run-show-die-info2.sh run-get-files.sh \
	run-get-lines.sh run-get-pubnames.sh run-get-aranges.sh \
	run-show-ciefde.sh run-show-abbrev.sh hash asm-tst1 asm-tst2 \
	asm-tst3 asm-tst4 asm-tst5 asm-tst6 asm-tst7 asm-tst8 msg_tst

EXTRA_DIST = run-arextract.sh run-arsymtest.sh run-firstdie.sh \
	     run-show-dies.sh run-show-tags.sh run-show-die-info1.sh \
	     run-show-die-info2.sh run-get-files.sh run-get-lines.sh \
	     run-get-pubnames.sh run-get-aranges.sh run-get-cus.sh \
	     run-show-ciefde.sh run-show-abbrev.sh

arextract_LDADD = ../libelf/libelf.la
arsymtest_LDADD = ../libelf/libelf.la
newfile_LDADD = ../libelf/libelf.la
saridx_LDADD = ../libelf/libelf.la
scnnames_LDADD = ../libelf/libelf.la
sectiondump_LDADD = ../libelf/libelf.la
showptable_LDADD = ../libelf/libelf.la
update1_LDADD = ../libelf/libelf.la
update2_LDADD = ../libelf/libelf.la
update3_LDADD = ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
update4_LDADD = ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
test_nlist_LDADD = ../libelf/libelf.la
get_cus_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
firstdie_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_dies_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_tags_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_die_info1_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_die_info2_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
get_lines_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
get_files_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
get_pubnames_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
get_aranges_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_ciefde_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
show_abbrev_LDADD = ../libdwarf/libdwarf.la ../libelf/libelf.la
hash_LDADD = ../libelf/libelf.la
asm_tst1_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst2_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst3_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst4_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst5_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst6_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst7_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
asm_tst8_LDADD = ../libasm/libasm.la ../libebl/libebl.la ../libelf/libelf.la
msg_tst_LDADD = ../libelf/libelf.la

CLEANFILES = xxx
