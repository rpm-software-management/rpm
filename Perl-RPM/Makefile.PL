use Config;
use ExtUtils::MakeMaker;

%XS = qw(
         RPM.xs             RPM.c
         RPM/Constants.xs   RPM/Constants.c
         RPM/Database.xs    RPM/Database.c
         RPM/Header.xs      RPM/Header.c
        );

@OBJECT = values %XS;
for (@OBJECT)
{
    s/\.c\b/\$(OBJ_EXT)/g;
}
$OBJECT = join(' ', @OBJECT);

%PM = map { ($from = $_) =~ s/xs$/pm/;
            ($from, "\$(INST_LIBDIR)/$from"); } (sort keys %XS);
$PM{q(RPM/Error.pm)} = '$(INST_LIBDIR)/RPM/Error.pm';

# This shouldn't be necessary, I don't think, but for now it is
unlink 'RPM/typemap';
symlink '../typemap', 'RPM/typemap';

# Cruft that MakeMaker wouldn't inherently know about
$CLEAN = join(' ', values %XS) . " $OBJECT";
$CLEAN .= ' pod2html-* */pod2html-* *.html */*.html RPM/typemap';

#
# Check for non-Perl dependancies:
#
print "Checking for required software components...\n";
@packages = `rpm -qa`;
unless (grep(/rpm\-devel/, @packages) && grep(/popt/, @packages))
{
    die "The Perl-RPM package relies on both the popt and rpm-devel packages
being installed. Stopped";
}

WriteMakefile(
              NAME         => 'RPM',
              DISTNAME     => 'Perl-RPM',
              VERSION_FROM => 'RPM.pm',
              LIBS         => '-lrpm -lpopt',
              DEFINE       => '',
              PM           => \%PM,
              XS           => \%XS,
              OBJECT       => $OBJECT,
              INC          => '-I/usr/include/rpm',
              DIR          => [],
              dist         => { COMPRESS => 'gzip -9f' },
              clean        => { FILES => $CLEAN },
             );

sub MY::c_o
{
    package MY; # so that "SUPER" works right

    my $text = shift->SUPER::c_o(@_);

    $text =~ s/CCCMD.*$/$& -o \$*\$(OBJ_EXT)/gm;

    $text;
}

sub MY::postamble
{
    join("\n",
         '',
         (map { "$_: RPM.h\n" } @OBJECT),
         '');
}
