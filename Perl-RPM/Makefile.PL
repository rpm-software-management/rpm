#!/bin/perl

# $Id: Makefile.PL,v 1.22 2001/04/29 22:44:13 rjray Exp $

use Config;
use ExtUtils::MakeMaker;
use Cwd 'cwd';

$cwd = cwd;

%XS = qw(
         RPM.xs             RPM.c
         RPM/Constants.xs   RPM/Constants.c
         RPM/Database.xs    RPM/Database.c
         RPM/Error.xs       RPM/Error.c
         RPM/Header.xs      RPM/Header.c
        );
#         RPM/Package.xs     RPM/Package.c

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
$CLEAN .= ' Perl-RPM.spec pod2html-* */pod2html-* *.html */*.html RPM/typemap';
$CLEAN .= ' rpmrc rpmmacro Perl-RPM-*';

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
@rpm_version = split(/\./, $rpm_version);
push(@rpm_version, '0') unless (@rpm_version > 2);
$rpm_version = sprintf '0x%02x'.'%02x'x$#rpm_version, @rpm_version;
die "RPM version must be at least 3.0.4, due to I/O features not available
prior to that version. Stopped"
    # The stringification is because Perl is still treating $rpm_version as a
    # string
    unless ($rpm_version >= "0x030004");
# Starting with RPM 4.0, there are a number of subtle-but-significant changes
$rpm_defines = "-DRPM_MAJOR=$rpm_version[0] -DRPM_MINOR=$rpm_version[1]" .
    " -DRPM_PATCH=$rpm_version[2] -DRPM_VERSION=$rpm_version";
if ($rpm_version >= "0x040000")
{
    $rpm_libs = '-lrpm -lrpmio -lpopt';
}
else
{
    $rpm_libs = '-lrpm -lpopt';
}

# Check for needed header files
$rpm_libdir = '';
for (qw(/usr/include /usr/local/include /opt/include /opt/rpm/include))
{
    next unless -d $_ and -d "$_/rpm";
    $rpm_libdir = "$_/rpm", last if (-d "$_/rpm" && -e "$_/rpm/rpmlib.h");
}
die "Header file 'rpmlib.h' not found. This is needed for compiling the XS glue
code. If this is present, edit $0 and add the correct directory to the search
list near line 78. Stopped"
    unless $rpm_libdir;

WriteMakefile(
              NAME         => 'RPM',
              DISTNAME     => 'Perl-RPM',
              VERSION      => '0.31',
              LIBS         => $rpm_libs,
              DEFINE       => $rpm_defines,
              PM           => \%PM,
              XS           => \%XS,
              EXE_FILES    => [ @SAMPLES ],
              OBJECT       => $OBJECT,
              INC          => "-I. -I$rpm_libdir",
              DIR          => [],
#              XSOPT        => '-noversioncheck -nolinenumbers',
              XSOPT        => '-noversioncheck',
              dist         => { COMPRESS => 'gzip -9f' },
              clean        => { FILES => $CLEAN }
             );

exit;

sub MY::c_o
{
    package MY; # so that "SUPER" works right

    my $text = shift->SUPER::c_o(@_);

    $text =~ s/CCCMD.*$/$& -o \$*\$(OBJ_EXT)/gm;

    $text;
}

sub MY::tools_other
{
    package MY;

    my $text = shift->SUPER::tools_other(@_);

    "$text\nECHO=\@echo\nRPM=$::rpm_binary\n";
}

sub MY::post_constants
{
    my $self = shift;

    "SPECFILE=$self->{DISTNAME}.spec";
}

sub MY::postamble
{
    my $self = shift;

    my @text;

    #
    # Generate the spec file from <DATA> with substitutions
    #
    print "Generating $self->{DISTNAME}.spec...\n";
    $self->{RELEASE} = 1 unless $self->{RELEASE};
    $self->{INSTALLDIR} = $Config{installsitearch} unless $self->{INSTALLDIR};
    open(SPEC, "> $self->{DISTNAME}.spec");
    if ($?)
    {
        die "Error creating spec file: $!, stopped";
    }
    else
    {
        for (<DATA>)
        {
            s/(SPEC_(\w+))/$self->{$2}/eg;
            print SPEC $_;
        }
        close(SPEC);
    }
    print "Generating rpmrc...\n";
    open(RPMRC, "> rpmrc");
    if ($?)
    {
        die "Error creating rpmrc file: $!, stopped";
    }
    else
    {
        print RPMRC <<END_rpmrc;
include: /usr/lib/rpm/rpmrc
macrofiles: /usr/lib/rpm/macros:rpmmacro
END_rpmrc
        close(RPMRC);
    }
    print "Generating rpmmacros...\n";
    open(MACRO, "> rpmmacro");
    if ($?)
    {
        die "Error creating macro file: $!, stopped";
    }
    else
    {
        print MACRO <<END_macro;
%_rpmfilename           %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm
%_builddir              %{_topdir}
%_sourcedir             %{_topdir}
%_specdir               %{_topdir}
%_srcrpmdir             %{_topdir}
%_rpmdir                %{_topdir}
%_topdir                $cwd
END_macro
        close(MACRO);
    }

    # Create the rules that create RPM and SRPM files
    push(@text, qq{
rpm: \$(SPECFILE) \$(DISTVNAME).tar\$(SUFFIX)
\t\$(RPM) -bb --clean --rcfile rpmrc \$(SPECFILE)

srpm: \$(SPECFILE) \$(DISTVNAME).tar\$(SUFFIX)
\t\$(RPM) -bs --clean --rcfile rpmrc \$(SPECFILE)
});

    # Create per-object-file dependancy on RPM.h
    push(@text, ('',
                 (map { "$_: RPM.h\n" } @OBJECT),
                 ''));

    join("\n", @text);
}

__DATA__
Summary: Perl bindings to the rpmlib API
Name: SPEC_DISTNAME
Version: SPEC_VERSION
Release: SPEC_RELEASE
Epoch: 1
Vendor: Randy J. Ray
Copyright: Artistic
Group: Applications/CPAN
Source: SPEC_DISTNAME-SPEC_VERSION.tar.gz
Url: http://www.blackperl.com/SPEC_DISTNAME
BuildRequires: perl >= 0:5.00503
Requires: perl >= 0:5.00503
Provides: SPEC_DISTNAME = SPEC_VERSION
BuildRoot: %{_tmppath}/%{name}-root

%description
Native Bindings to the RPM API for Perl 5.005 and later. Uses a combination
of Perl and C/XS (compiled) code to provide access to the RPM database,
headers of installed packages, and more.

%prep
%setup -q -n SPEC_DISTNAME-%{version} 

%build
CFLAGS="$RPM_OPT_FLAGS" perl Makefile.PL

%clean 
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT
install_dir=SPEC_INSTALLDIR
mkdir -p $RPM_BUILD_ROOT/$installdir
make PREFIX=$RPM_BUILD_ROOT/usr install

[ -x /usr/lib/rpm/brp-compress ] && /usr/lib/rpm/brp-compress

find $RPM_BUILD_ROOT/usr -type f -print |
        sed "s@^$RPM_BUILD_ROOT@@g" |
        grep -v perllocal.pod |
        grep -v "\.packlist" > SPEC_DISTNAME-SPEC_VERSION-filelist
if [ "$(cat SPEC_DISTNAME-SPEC_VERSION-filelist)X" = "X" ] ; then
    echo "ERROR: EMPTY FILE LIST"
    exit -1
fi

%files -f SPEC_DISTNAME-SPEC_VERSION-filelist
%defattr(-,root,root)

%changelog
* Tue Oct 10 2000 Randy J. Ray <rjray@blackperl.com>
- Turned into a meta-file that gets filled in by MakeMaker

* Tue Oct  3 2000 Jeff Johnson <jbj@redhat.com>
- update to 0.28.

* Mon Oct  2 2000 root <root@redhat.com>
- Spec file was autogenerated.
