## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits

EXTRA_DIST = new-bu.sgml

CLEANFILES = new-bu.dvi

# We need only a few special rules to generate the various output formats
# from the SGML sources.
.PHONY: dvi pdf html
dvi: $(srcdir)new-bu.dvi
pdf: $(srcdir)new-bu.pdf

$(srcdir)%.dvi: %.sgml
	db2dvi $^
$(srcdir)%.pdf: %.sgml
	db2pdf $^
