use ExtUtils::MakeMaker;

%XS = qw(
         RPM.xs                     RPM.c
         Constants/Constants.xs     Constants/Constants.c
         Database/Database.xs       Database/Database.c
         Header/Header.xs           Header/Header.c
        );

@OBJECT = values %XS;
for (@OBJECT)
{
    s/\.c\b/\$(OBJ_EXT)/g;
}
$OBJECT = join(' ', @OBJECT);

@DIRS = qw(Constants Header Database);

%PM = map { ($from = $_) =~ s/xs$/pm/;
            ($to = $from) =~ s|.*?/|RPM/|;
            ($from, "\$(INST_LIBDIR)/$to"); } (sort keys %XS);

# Make symlinks for typemap in the sub-dirs
for (@DIRS)
{
    symlink '../typemap', "$_/typemap";
}

# Cruft that MakeMaker wouldn't inherently know about
$CLEAN = join(' ', map { "$_/typemap" } @DIRS) . " $OBJECT";
$CLEAN .= ' pod2html-* */pod2html-* *.html */*.html';

WriteMakefile(
              NAME         => 'RPM',
              DISTNAME     => 'Perl-RPM',
              VERSION_FROM => 'RPM.pm',
              LIBS         => '-lrpm -lpopt',
              DEFINE       => '',
              XS           => \%XS,
              PM           => \%PM,
              OBJECT       => $OBJECT,
              INC          => '-I/usr/include/rpm',
              dist         => { COMPRESS => 'gzip -9f' },
              clean        => { FILES => $CLEAN },
              #realclean    => { FILES => $CLEAN },
             );

sub MY::c_o
{
    package MY; # so that "SUPER" works right

    my $text = shift->SUPER::c_o(@_);
    $text =~ s/CCCMD.*$/$&\n\t\@if test -f `basename \$*.o` -a "`basename \$*.o`" != "\$*.o"; then mv `basename \$*.o` \$*.o; fi/m;

    $text;
}

sub MY::postamble
{
    join("\n",
         '',
         (map { "$_: RPM.h\n" } @OBJECT),
         '');
}
