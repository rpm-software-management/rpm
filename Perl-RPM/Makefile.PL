use Config;
use ExtUtils::MakeMaker;

%XS = qw(
         RPM.xs             RPM.c
         RPM/Constants.xs   RPM/Constants.c
         RPM/Database.xs    RPM/Database.c
         RPM/Error.xs       RPM/Error.c
         RPM/Header.xs      RPM/Header.c
        );

@OBJECT = values %XS;
for (@OBJECT)
{
    s/\.c\b/\$(OBJ_EXT)/g;
}
$OBJECT = join(' ', @OBJECT);

%PM = map { ($from = $_) =~ s/xs$/pm/;
            ($from, "\$(INST_LIBDIR)/$from"); } (keys %XS);

@SAMPLES = qw(utils/rpmprune);

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
$rpm_binary = '';
for (split(':', $ENV{PATH}))
{
    $rpm_binary = "$_/rpm", last
        if (-e "$_/rpm" && -x _ && ! -d _);
}
die "Executable 'rpm' not found. This is needed for fetching configuration
information, stopped"
    unless $rpm_binary;

chomp($rpm_version = qx($rpm_binary --version));
$rpm_version = (reverse split(/ /, $rpm_version))[0];
$rpm_version = join('', map { sprintf "%03d", $_ } split(/\./, $rpm_version));
$rpm_version =~ s/^0*//;
die "RPM version must be at least 3.0.4, due to I/O features not available
prior to that version. Stopped"
    unless ($rpm_version >= 3000004);

# Check for needed header files
$rpm_libdir = '';
for (qw(/usr/include /usr/local/include /opt/include /opt/rpm/include))
{
    next unless -d $_ and -d "$_/rpm";
    $rpm_libdir = "$_/rpm", last if (-d "$_/rpm" && -e "$_/rpm/rpmlib.h");
}
die "Header file 'rpmlib.h' not found. This is needed for compiling the XS glue
code. If this is present, edit $0 and add the correct directory to the search
list at line 58. Stopped"
    unless $rpm_libdir;

WriteMakefile(
              NAME         => 'RPM',
              DISTNAME     => 'Perl-RPM',
              VERSION_FROM => 'RPM.pm',
              LIBS         => '-lrpm -lpopt',
              DEFINE       => '',
              PM           => \%PM,
              XS           => \%XS,
              EXE_FILES    => [ @SAMPLES ],
              OBJECT       => $OBJECT,
              INC          => "-I$rpm_libdir",
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
