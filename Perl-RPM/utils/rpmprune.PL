#!/usr/bin/perl

use Config;
use File::Basename qw(&basename &dirname);

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
chdir(dirname($0));
($file = basename($0)) =~ s/\.PL$//;
$file =~ s/\.pl$//
	if ($Config{'osname'} eq 'VMS' or
	    $Config{'osname'} eq 'OS2');  # "case-forgiving"

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{'startperl'}
    eval 'exec perl -S \$0 "\$@"'
	if 0;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

use strict;
use vars qw($USAGE $VERSION $revision $cmd %opts %RPM $file $hdr $installed
            $ret);

use Getopt::Long;
use File::Basename qw(basename);

use RPM::Database;
use RPM::Header;

$cmd = basename $0;
$revision = q$Revision: 1.2 $;
$VERSION = sprintf("%d.%02d", $revision =~ /(\d+)\.(\d+)/);
$USAGE = "USAGE:
    $cmd [ --newer ] [ --older ] [ --equal ] [ --uninst ]
        [ --invert ] file [ file ... ]

Where:

--newer (--nonewer)     Show (do not show) those files which are newer
--older (--noolder)     Show (do not show) those that are older
--equal (--noequal)     Show (do not show) those that match the database
--uninst (--nouninst)   Show (do not show) those that are not in the database

--invert                Invert the given selection logic

The default is:

    --newer --noolder --noequal --uninst
";

$opts{newer} = 1;
$opts{older} = 0;
$opts{equal} = 0;
$opts{uninst} = 1;
GetOptions(\%opts, qw(help newer! older! equal! uninst! invert)) or
    die "$USAGE\nStopped";

if ($opts{help})
{
    print $USAGE;
    exit 0;
}
if ($opts{invert})
{
    $opts{$_} = 1 - $opts{$_} for (qw(newer older equal uninst));
}

tie %RPM, "RPM::Database";

for $file (@ARGV)
{
    $hdr = new RPM::Header $file;
    unless ($hdr)
    {
        warn "$file: $RPM::err\n";
        next;
    }

    $installed = $RPM{$hdr->{name}->[0]};
    unless ($installed)
    {
        # It isn't currently installed, so there is no conflict
        print "$file\n" if $opts{uninst};
        undef $hdr;
        next;
    }

    $ret = $hdr->cmpver($installed);
    print "$file\n" if ($ret < 0 and $opts{older});
    print "$file\n" if ($ret == 0 and $opts{equal});
    print "$file\n" if ($ret > 0 and $opts{newer});
    undef $hdr;
}

exit;

__END__

=head1 NAME

rpmprune - Remove unneeded files from a list of RPM package files

=head1 SYNOPSIS

rpmprune [ --newer ] [ --older ] [ --equal ] [ --uninst ]
    [ --invert ] filelist ...

=head1 DESCRIPTION

The B<rpmprune> tool is a simple example of using some of the B<RPM> Perl
bindings.

With B<rpm> version 3 and newer, multiple files given on the command-line
for an install, uninstall or update command are treated as a single
I<transaction>. If any of the files in the set cannot be acted upon, the
whole transaction must be rejected. This is inconvenient for casual package
upgrades where a directory may have many B<rpm> files, some of which are
already installed. A command of:

    rpm -Uhv *.rpm

would fail, as one (or more) files in the set is already installed. This
can lead to careless use of options such as C<--force>.

In the simplest usage, this tool eases that situation by allowing:

    rpm -Uhv `rpmprune *.rpm`

When the back-ticks are evaluated, B<rpmprune> has only echoed the names of
those files that are either newer than their installed counterparts, or are
not installed at all.

=head1 OPTIONS

For maximum flexibility, B<rpmprune> supports more options than anyone will
realistically put to use:

=over

=item --help

Print a summary of the options and general usage.

=item --newer (--nonewer)

If selected, all files that are newer than their installed counterparts
will be echoed. Prefix with "no" to suppress the printing of these.

=item --older (--noolder)

If selected, all files that are older than their installed counterparts
will be echoed. Prefix with "no" to suppress them, instead.

=item --equal (--noequal)

If selected, all files that are the same version as their installed
counterparts will be echoed. Prefix with "no" to suppress them, instead.

=item --uninst (--nouninst)

If selected, all files that are for packages not currently installed on
the system will be echoed. Prefix with "no" to suppress them, instead.

=item --invert

Invert the logic of all four of the previous selectors.

=back

Without any arguments, B<rpmprune> acts as if the following were the command
line:

    rpmprune --newer --noolder --noequal --uninst ...

The C<--invert> option is applied last, after all others have been processed.

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';

=cut
