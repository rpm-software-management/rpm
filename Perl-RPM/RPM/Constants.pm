###############################################################################
#
#   (c) Copyright @ 2000, Randy J. Ray <rjray@blackperl.com>
#               All Rights Reserved
#
###############################################################################
#
#   $Id: Constants.pm,v 1.18 2001/04/27 09:05:21 rjray Exp $
#
#   Description:    Constants for the RPM package
#
#   Functions:      None-- constants are implemented as pseudo-functions
#
#   Libraries:      RPM (to force bootstrapping)
#
###############################################################################

package RPM::Constants;

use strict;
use vars qw(@ISA @EXPORT_OK %EXPORT_TAGS $VERSION $AUTOLOAD);

require Exporter;

use RPM;

@ISA = qw(Exporter);

$VERSION = do { my @r=(q$Revision: 1.18 $=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };

@EXPORT_OK = qw(
                CHECKSIG_GPG
                CHECKSIG_MD5
                CHECKSIG_PGP
                INSTALL_HASH
                INSTALL_LABEL
                INSTALL_NODEPS
                INSTALL_NOORDER
                INSTALL_PERCENT
                INSTALL_UPGRADE
                QUERY_FOR_CONFIG
                QUERY_FOR_DOCS
                QUERY_FOR_DUMPFILES
                QUERY_FOR_LIST
                QUERY_FOR_STATE
                RPM_NULL_TYPE
                RPM_CHAR_TYPE
                RPM_INT8_TYPE
                RPM_INT16_TYPE
                RPM_INT32_TYPE
                RPM_STRING_TYPE
                RPM_BIN_TYPE
                RPM_STRING_ARRAY_TYPE
                RPM_I18NSTRING_TYPE
                RPMERR_BADARG
                RPMERR_BADDEV
                RPMERR_BADFILENAME
                RPMERR_BADMAGIC
                RPMERR_BADRELOCATE
                RPMERR_BADSIGTYPE
                RPMERR_BADSPEC
                RPMERR_CHOWN
                RPMERR_CPIO
                RPMERR_CREATE
                RPMERR_DBCORRUPT
                RPMERR_DBGETINDEX
                RPMERR_DBOPEN
                RPMERR_DBPUTINDEX
                RPMERR_EXEC
                RPMERR_FILECONFLICT
                RPMERR_FLOCK
                RPMERR_FORK
                RPMERR_GDBMOPEN
                RPMERR_GDBMREAD
                RPMERR_GDBMWRITE
                RPMERR_GZIP
                RPMERR_INTERNAL
                RPMERR_LDD
                RPMERR_MKDIR
                RPMERR_MTAB
                RPMERR_NEWPACKAGE
                RPMERR_NOCREATEDB
                RPMERR_NOGROUP
                RPMERR_NORELOCATE
                RPMERR_NOSPACE
                RPMERR_NOSPEC
                RPMERR_NOTSRPM
                RPMERR_NOUSER
                RPMERR_OLDDB
                RPMERR_OLDDBCORRUPT
                RPMERR_OLDDBMISSING
                RPMERR_OLDPACKAGE
                RPMERR_PKGINSTALLED
                RPMERR_READERROR
                RPMERR_RENAME
                RPMERR_RMDIR
                RPMERR_RPMRC
                RPMERR_SCRIPT
                RPMERR_SIGGEN
                RPMERR_STAT
                RPMERR_UNKNOWNARCH
                RPMERR_UNKNOWNOS
                RPMERR_UNLINK
                RPMERR_UNMATCHEDIF
                RPMFILE_CONFIG
                RPMFILE_DOC
                RPMFILE_DONOTUSE
                RPMFILE_GHOST
                RPMFILE_LICENSE
                RPMFILE_MISSINGOK
                RPMFILE_NOREPLACE
                RPMFILE_README
                RPMFILE_SPECFILE
                RPMFILE_STATE_NETSHARED
                RPMFILE_STATE_NORMAL
                RPMFILE_STATE_NOTINSTALLED
                RPMFILE_STATE_REPLACED
                RPMPROB_FILTER_DISKSPACE
                RPMPROB_FILTER_FORCERELOCATE
                RPMPROB_FILTER_IGNOREARCH
                RPMPROB_FILTER_IGNOREOS
                RPMPROB_FILTER_OLDPACKAGE
                RPMPROB_FILTER_REPLACENEWFILES
                RPMPROB_FILTER_REPLACEOLDFILES
                RPMPROB_FILTER_REPLACEPKG
                RPMSENSE_EQUAL
                RPMSENSE_GREATER
                RPMSENSE_LESS
                RPMSENSE_OBSOLETES
                RPMSENSE_PREREQ
                RPMSENSE_SENSEMASK
                RPMSENSE_TRIGGER
                RPMSENSE_TRIGGERIN
                RPMSENSE_TRIGGERPOSTUN
                RPMSENSE_TRIGGERUN
                RPMSIGTAG_GPG
                RPMSIGTAG_LEMD5_1
                RPMSIGTAG_LEMD5_2
                RPMSIGTAG_MD5
                RPMSIGTAG_PGP
                RPMSIGTAG_PGP5
                RPMSIGTAG_SIZE
                RPMSIG_BAD
                RPMSIG_NOKEY
                RPMSIG_NOTTRUSTED
                RPMSIG_OK
                RPMSIG_UNKNOWN
                RPMTAG_ARCH
                RPMTAG_ARCHIVESIZE
                RPMTAG_BASENAMES
                RPMTAG_BUILDARCHS
                RPMTAG_BUILDHOST
                RPMTAG_BUILDMACROS
                RPMTAG_BUILDROOT
                RPMTAG_BUILDTIME
                RPMTAG_CAPABILITY
                RPMTAG_CHANGELOGNAME
                RPMTAG_CHANGELOGTEXT
                RPMTAG_CHANGELOGTIME
                RPMTAG_CONFLICTFLAGS
                RPMTAG_CONFLICTNAME
                RPMTAG_CONFLICTVERSION
                RPMTAG_COPYRIGHT
                RPMTAG_COOKIE
                RPMTAG_DESCRIPTION
                RPMTAG_DIRINDEXES
                RPMTAG_DIRNAMES
                RPMTAG_DISTRIBUTION
                RPMTAG_EXCLUDEARCH
                RPMTAG_EXCLUDEOS
                RPMTAG_EXCLUSIVEARCH
                RPMTAG_EXCLUSIVEOS
                RPMTAG_FILEDEVICES
                RPMTAG_FILEFLAGS
                RPMTAG_FILEGROUPNAME
                RPMTAG_FILEINODES
                RPMTAG_FILELANGS
                RPMTAG_FILELINKTOS
                RPMTAG_FILEMD5S
                RPMTAG_FILEMODES
                RPMTAG_FILEMTIMES
                RPMTAG_FILERDEVS
                RPMTAG_FILESIZES
                RPMTAG_FILESTATES
                RPMTAG_FILEUSERNAME
                RPMTAG_FILEVERIFYFLAGS
                RPMTAG_GIF
                RPMTAG_GROUP
                RPMTAG_ICON
                RPMTAG_INSTALLTIME
                RPMTAG_INSTPREFIXES
                RPMTAG_LICENSE
                RPMTAG_NOPATCH
                RPMTAG_NOSOURCE
                RPMTAG_NAME
                RPMTAG_OBSOLETEFLAGS
                RPMTAG_OBSOLETENAME
                RPMTAG_OBSOLETEVERSION
                RPMTAG_OS
                RPMTAG_PACKAGER
                RPMTAG_PATCH
                RPMTAG_POSTIN
                RPMTAG_POSTINPROG
                RPMTAG_POSTUN
                RPMTAG_POSTUNPROG
                RPMTAG_PREFIXES
                RPMTAG_PREIN
                RPMTAG_PREINPROG
                RPMTAG_PREUN
                RPMTAG_PREUNPROG
                RPMTAG_PROVIDEFLAGS
                RPMTAG_PROVIDENAME
                RPMTAG_PROVIDEVERSION
                RPMTAG_RELEASE
                RPMTAG_REQUIREFLAGS
                RPMTAG_REQUIRENAME
                RPMTAG_REQUIREVERSION
                RPMTAG_RPMVERSION
                RPMTAG_SIZE
                RPMTAG_SOURCE
                RPMTAG_SOURCERPM
                RPMTAG_SUMMARY
                RPMTAG_TRIGGERCONDS
                RPMTAG_TRIGGERFLAGS
                RPMTAG_TRIGGERINDEX
                RPMTAG_TRIGGERNAME
                RPMTAG_TRIGGERSCRIPTPROG
                RPMTAG_TRIGGERSCRIPTS
                RPMTAG_TRIGGERVERSION
                RPMTAG_URL
                RPMTAG_VENDOR
                RPMTAG_VERIFYSCRIPT
                RPMTAG_VERIFYSCRIPTPROG
                RPMTAG_VERSION
                RPMTAG_XPM
                RPMTRANS_FLAG_ALLFILES
                RPMTRANS_FLAG_BUILD_PROBS
                RPMTRANS_FLAG_JUSTDB
                RPMTRANS_FLAG_KEEPOBSOLETE
                RPMTRANS_FLAG_NODOCS
                RPMTRANS_FLAG_NOSCRIPTS
                RPMTRANS_FLAG_NOTRIGGERS
                RPMTRANS_FLAG_TEST
                RPMVERIFY_ALL
                RPMVERIFY_FILESIZE
                RPMVERIFY_GROUP
                RPMVERIFY_LINKTO
                RPMVERIFY_LSTATFAIL
                RPMVERIFY_MD5
                RPMVERIFY_MODE
                RPMVERIFY_MTIME
                RPMVERIFY_NONE
                RPMVERIFY_RDEV
                RPMVERIFY_READFAIL
                RPMVERIFY_READLINKFAIL
                RPMVERIFY_USER
                UNINSTALL_ALLMATCHES
                UNINSTALL_NODEPS
                VERIFY_DEPS
                VERIFY_FILES
                VERIFY_MD5
                VERIFY_SCRIPT
               );

#
# To create the %EXPORT_TAGS table, we're going to create a temp hash with
# the tags broken down into groupings. Then when the "known" groupings are
# done, whatever is left can go in "misc"
#
my %groups = ();
my %consts = map { $_, 1 } @EXPORT_OK;
for my $group (qw(install query rpmerr rpmfile rpmlead rpmmess rpmprob_filter
                  rpmsense rpmsigtag rpmsig rpmtag rpmtrans_flag
                  rpmverify uninstall verify))
{
    my $pat = qr/^$group/i;
    my $list = [];

    for (grep($_ =~ $pat, sort keys %consts))
    {
        push(@$list, $_);
        delete $consts{$_};
    }

    $groups{$group} = $list;
}

# Types didn't fit neatly into the above logic-loop
$groups{rpmtype} = [];
for (grep($_ =~ /^RPM_.*_TYPE/, sort keys %consts))
{
    push(@{$groups{rpmtype}}, $_);
    delete $consts{$_};
}

# Pick up any stragglers
$groups{misc} = [ sort keys %consts ];

# Merge the install and uninstall groups
push(@{$groups{install}}, @{$groups{uninstall}});
delete $groups{uninstall};

%EXPORT_TAGS = (
                all => [ @EXPORT_OK ],
                %groups
               );

sub AUTOLOAD
{
    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    die "& not defined" if $constname eq 'constant';
    my $val = constant($constname);
    if ($! != 0)
    {
        die "Your vendor has not defined RPM macro $constname";
    }
    no strict 'refs';
    *$AUTOLOAD = sub { $val };
    goto &$AUTOLOAD;
}

1;

__END__

=head1 NAME

RPM::Constants - Groups of RPM-defined symbols

=head1 SYNOPSIS

    use RPM::Constants qw(:rpmerr :rpmtype);

=head1 DESCRIPTION

This package is a collection of the constants defined by B<rpm> itself that
may be of use to those developing with the B<RPM> Perl bindings.

=head1 GROUPS

For ease of use and uderstanding (at last count, the total number of
constants was 232), the constants are broken up into several smaller groups:

=head2 Header Tag Identifiers

The following symbols may be imported via the tag B<:rpmtag>, and represent
the various elements that may be present in a package header. When used to
retrieve data from a header as a hash key, the C<RPMTAG_> portion should be
omitted from the name. Use the full name only when referring to the constant.
Note that each name is followed by either a C<$> or a C<@>. This signifies
the return type of the data; a scalar or an array reference. In all cases, a
failed operation is noted by a return value of C<undef>.

The majority of the tags that return list references in fact refer to the
ordered list of files present in the C<BASENAMES> tag. In these cases (such
as C<MD5SUMS>), the value of the array at a given point may be null if it
is not relevant. That is because the C<BASENAMES> array (and thus all other
file-related lists) must accomodate the indices at which a directory name is
specified for the sake of defining the directory. In such cases, values such
as size or MD5 checksum have no direct relevance.

=over

=item RPMTAG_ARCH ($)

Name of the architecture that the package was built for. If the package
is architecture-independant, the value should read "noarch".

=item RPMTAG_ARCHIVESIZE ($)

Size of the archive portion of the file. B<RPM> stores the archive portion
of a (non-source) package as a B<cpio> archive, which may also be compressed
internally. Thus, this value is generally larger than the file size of the
C<RPM> file itself.

=item RPMTAG_BASENAMES (@)

A list of the base (leaf) names of the files contained within the package.
These are combined with the values from B<RPMTAG_DIRNAMES> using a mapping
provided by B<RPMTAG_DIRINDEXES>.

This is actually a very key tag within a header. Many of the list-returning
tags documented further down maintain a one-to-one correlation with the
elements in this array.

=item RPMTAG_BUILDARCHS (@)

Not entirely sure. Appears from source code examples to be a list of those
architectures for which a package should be built. All examples from the set
of SRPMs in Red Hat Linux 6.2 only use this tag when the only value is
C<noarch>.

=item RPMTAG_BUILDHOST ($)

Name of the host the package was built on.

=item RPMTAG_BUILDMACROS (@)

This does not seem to be used in the library. It may be present for future
expansion use.

=item RPMTAG_BUILDROOT ($)

Specifies the root at which the package is built.

=item RPMTAG_BUILDTIME ($)

The time/date when the package was created, expressed as a C<time()> value
(seconds since the epoch).

=item RPMTAG_CHANGELOGNAME (@)

=item RPMTAG_CHANGELOGTEXT (@)

=item RPMTAG_CHANGELOGTIME (@)

These three items should be taken together. Each should have the same number
of items, and the items at corresponding indices should be taken together.
Taken this way, they provide a small-scale changelog for the package, detailing
the name of the person making the entry, the text of the entry and the time
of the entry, in the respective order given above.

=item RPMTAG_CONFLICTFLAGS (@)

=item RPMTAG_CONFLICTNAME (@)

=item RPMTAG_CONFLICTVERSION (@)

These three items are used in conjunction to specify packages and/or
individual files which the package itself would conflict with. Of the three,
only B<RPMTAG_CONFLICTNAME> is required to have data in all elements of
the array.  The other two will have the same number of elements, though some
(or most) may be null. This is the same approach as is used to specify the
elements that the package obsoletes, those the package provides and those
the package requires (see L<"Three-Part Linkage"> below).

=item RPMTAG_COPYRIGHT

Maintained by RPM for backwards-compatibility with some older packages. It
is the same as C<RPMTAG_LICENSE>.

=item RPMTAG_COOKIE ($)

A simple tag, a single text string, added at the time the RPM is created.
Generally, it is created from the hostname on which the package is built
and the UNIX C<time()> value at the time of packaging.

=item RPMTAG_DESCRIPTION ($)

A textual description of the package.

=item RPMTAG_DIRINDEXES (@)

This data should have a one-to-one correspondance with B<RPMTAG_BASENAMES>,
above. Each item here is a numerical index into the list of directories named
in B<RPMTAG_DIRNAMES> below. It indicates which of the directories is to be
prepended to the corresponding base file name in order to create the full
pathname.

=item RPMTAG_DIRNAMES (@)

This is a list of all directories into which the package would install files.
This list is used with B<RPMTAG_BASENAMES> to create full paths, indexed by
way of B<RPMTAG_DIRINDEXES> above.

=item RPMTAG_DISTRIBUTION ($)

A text label identifying the name given to the overall larger distribution
the package itself is a part of.

=item RPMTAG_EXCLUDEARCH (@)

A list of architectures for which the package should not be built.

=item RPMTAG_EXCLUDEOS (@)

A list of operating systems for which the package should not be built.

=item RPMTAG_EXCLUSIVEARCH (@)

A list of architectures only for which the package should be built.

=item RPMTAG_EXCLUSIVEOS (@)

A list of operating systems only for which the package should be built.

=item RPMTAG_FILEDEVICES (@)

The integer device values (from the B<stat> system call) for each file in
the package.

=item RPMTAG_FILEFLAGS (@)

A bit-field with zero or more of the flags defined below under the heading
of I<rpmfile>. See the flags themselves for more detail.

=item RPMTAG_FILEGROUPNAME (@)

A string-array data field that contains the group ID (by name) that should
be used for setting group ownership of the files contained in the package.
There should be a one-to-one correspondance between this list and the list of
files in C<RPMTAG_BASENAMES>. See also C<RPMTAG_USERNAME>.

=item RPMTAG_FILEINODES (@)

The C<inode> (from the B<stat> system call) that each file in
the package had on the system on which the package was built.

=item RPMTAG_FILELANGS (@)

Used to specify language-specific files, which may then be marked for skipping
based on the list of accepted languages at install-time.

=item RPMTAG_FILELINKTOS (@)

A list of names with exactly as many elements as there are filenames; each
slot in this list is either empty, or (if not) gives the name of a file that
the current filename should be made as a symbolic link to.

=item RPMTAG_FILEMD5S (@)

MD5 checksums for each file in the package.

=item RPMTAG_FILEMODES (@)

The file-modes as integer values, for each file in the package.

=item RPMTAG_FILEMTIMES (@)

The integer modification-time (from the B<stat> system call) for each file in
the package.

=item RPMTAG_FILERDEVS (@)

The integer C<rdev> values (from the B<stat> system call) for each file in
the package.

=item RPMTAG_FILESIZES (@)

The size (in bytes) of each file in the package.

=item RPMTAG_FILESTATES (@)

A list of file-state information for each file in the package. References
the constants defined below under the heading of C<rpmfile_states>.

=item RPMTAG_FILEUSERNAME (@)

A string-array data field that contains the user ID (by name) that should
be used for setting ownership of the files contained in the package. There
should be a one-to-one correspondance between this list and the list of
files in C<RPMTAG_BASENAMES>. See also C<RPMTAG_GROUPNAME>.

=item RPMTAG_FILEVERIFYFLAGS (@)

A list of flags (implemented as a bit-field within an integer) for each file
in the archive, specifying what should be checked during the verification
stage. See the B<RPMVERIFY_*> constants below.

=item RPMTAG_GIF ($)

Similar to B<RPMTAG_ICON> defined below, with the restriction that the file
specified should in fact be a GIF image.

=item RPMTAG_GROUP ($)

A one-line text string that places the package within the overall hierarchy
of packages, using a UNIX-style format of denoting level with forward-slash
characters (C</>). Most packages will have at least two elements separated by
one such slash, though more are possible (as is a top-level name).

=item RPMTAG_ICON ($)

Specifies a file within a source-RPM (SRPM) that should be treated as an icon
(of either GIF or XPM format), for potential use by GUI-based RPM tools.
See C<RPMTAG_XPM> below and C<RPMTAG_GIF> above.

=item RPMTAG_INSTALLTIME ($)

The time at which the package was installed on your system. Should only be
present in header objects from the database, not from uninstalled packages.

=item RPMTAG_INSTPREFIXES (@)

Specifies one or more prefixes that are set to the environment variables,
C<RPM_INSTALL_PREFIX{n}>, where C<{n}> is a number starting from zero. These
are set before executing any of the scripts (pre- or post-install, or verify).

=item RPMTAG_LICENSE ($)

The license and/or restrictions under which the package is distributed.

=item RPMTAG_NAME ($)

The name of the package. This is the first part of a triple used to uniquely
identify a given package. It is used in conjunction with B<RPMTAG_VERSION>
and B<RPMTAG_RELEASE>, in that order.

=item RPMTAG_NOPATCH (@)

=item RPMTAG_NOSOURCE (@)

These are used to list elements that should not be included in the resulting
SRPM when it is built from a spec-file. The lists provided by the B<SOURCE>
and B<PATCH> tags provide all elements as itemized in the spec-file. However,
if either of these tags are also present, then some elements may not actually
exist in the package. Both of these refer to entries in the corresponding
list of names by numberical index (starting at 0).

=item RPMTAG_OBSOLETEFLAGS (@)

=item RPMTAG_OBSOLETENAME (@)

=item RPMTAG_OBSOLETEVERSION (@)

These three items are used in conjunction to specify packages and/or
individual files which the package itself obsoletes. Of the three, only
B<RPMTAG_OBSOLETENAME> is required to have data in all elements of the array.
The other two will have the same number of elements, though some (or most)
may be null. This is the same approach as is used to specify the elements
that the package conflicts with, those the package provides and those the
package requires (see L<"Three-Part Linkage"> below).

=item RPMTAG_OS ($)

The name of the O/S for which the package is intended.

=item RPMTAG_PACKAGER ($)

Name of the group/company/individual who built the package.

=item RPMTAG_PATCH (@)

A list of patch files (see L<patch>) that will be applied to the source tree
when building the package from a source-RPM (SRPM). These files are part of
the bundle in the SRPM. All patch files listed in the original spec are listed
here, even if some were excluded by the B<NOPATCH> tag defined earlier.

=item RPMTAG_POSTIN (@)

Post-installation scripts, each entry in the list holds the text for a full
script.

=item RPMTAG_POSTINPROG (@)

The program (and additional arguments) for executing post-installation scripts.
The default is B</bin/sh> with no arguments. This is much like the C argv/argc
pair, in that list subscript 0 represents the program itself while the
remaining list items (if any) are arguments to the program.

=item RPMTAG_POSTUN (@)

Post-uninstallation scripts, again with one full script per array item.

=item RPMTAG_POSTUNPROG (@)

Specification of the program to run post-uninstallation scripts. See
B<RPMTAG_POSTINPROG>.

=item RPMTAG_PREFIXES (@)

The list of directory prefixes under which files are (or will be) installed.
This differs from the B<DIRNAMES> tag in that it is used to specify the parts
of the filesystem affected. Thus, it is generally a shorter list and the
elements are more basic (three directories under C</usr> in B<DIRNAMES> will
only warrant a mention of C</usr> in this tag).

=item RPMTAG_PREIN (@)

=item RPMTAG_PREINPROG (@)

=item RPMTAG_PREUN (@)

=item RPMTAG_PREUNPROG (@)

Specification of the scripts and commands to use in executing them, for
pre-installation and pre-uninstallation. See the B<RPMTAG_POST*> set above.

=item RPMTAG_PROVIDEFLAGS (@)

=item RPMTAG_PROVIDENAME (@)

=item RPMTAG_PROVIDEVERSION (@)

These three items are used in conjunction to specify the specific files that
the package itself provides to other packages as possible dependancies. Of the
three, only B<RPMTAG_PROVIDENAME> is required to have data in all elements
of the array.  The other two will have the same number of elements, though
some (or most) may be null. This three-part specification is also used to
itemize dependancies and obsoletions (see L<"Three-Part Linkage">).

=item RPMTAG_RELEASE ($)

The release part of the identifying triple for a package. This is combined
with the B<RPMTAG_NAME> and B<RPMTAG_VERSION> tags to create a unique
identification for each package.

=item RPMTAG_REQUIREFLAGS (@)

=item RPMTAG_REQUIRENAME (@)

=item RPMTAG_REQUIREVERSION (@)

These three items are used in conjunction to specify packages and/or
individual files on which the package itself depends. Of the three, only
B<RPMTAG_REQUIRENAME> is required to have data in all elements of the array.
The other two will have the same number of elements, though some (or most)
may be null. This is the same approach as is used to specify the elements
that the package provides and those the package obsoletes (see
L<"Three-Part Linkage">).

=item RPMTAG_RPMVERSION ($)

The version of B<rpm> used when bundling the package.

=item RPMTAG_SIZE ($)

Total size of the package contents, the sum of individual file sizes.

=item RPMTAG_SOURCE (@)

A list of the source files that are present in the SRPM package. All files
listed here will be placed in the relevant C<SOURCES> directory when building
from this SRPM. All source files listed in the original spec are listed here,
even if some were excluded by the B<NOSOURCE> tag defined earlier.

=item RPMTAG_SOURCERPM ($)

The source-RPM (SRPM) file used to build this package. If the file being
queried is itself a source-RPM, this tag will be non-existent or null in
value.

=item RPMTAG_SUMMARY ($)

A one line summary description of the package.

=item RPMTAG_TRIGGERCONDS (@)

=item RPMTAG_TRIGGERFLAGS (@)

=item RPMTAG_TRIGGERINDEX (@)

=item RPMTAG_TRIGGERNAME (@)

=item RPMTAG_TRIGGERSCRIPTPROG (@)

=item RPMTAG_TRIGGERSCRIPTS (@)

=item RPMTAG_TRIGGERVERSION (@)

These items are all taken together to manage the trigger functionality and
mechanism of the RPM package. This is covered in greater depth in a later
section (see L<"The Trigger Specifications">).

=item RPMTAG_URL ($)

A Uniform Resource Locator (generally a WWW page) for the vendor/individual
or for the software project itself.

=item RPMTAG_VENDOR ($)

An alternate identifier for the company that created and provided the package.

=item RPMTAG_VERIFYSCRIPT (@)

Scripts to be run during the verification stage. As with other script-providing
tags, each array element contains one full script.

=item RPMTAG_VERIFYSCRIPTPROG (@)

The program (and arguments) that is to be used in executing the verification
scripts. If absent or empty, C</bin/sh> with no arguments is used.

=item RPMTAG_VERSION ($)

The package version, the second part (with B<RPMTAG_NAME> and
B<RPMTAG_RELEASE>) of the triple used to uniquely identify packages.

=item RPMTAG_XPM ($)

The name of a file in the SRPM that may be used as an icon by a GUI-based
tool. This differs from B<RPMTAG_ICON> above in that it implies that the file
is specifically a XPM format image.

=back

=head2 Three-Part Linkage

There are several groupings of tags that are used to specify a linkage of
some sort, often external in nature. These triple-tags consist of a list of
textual names, a list of corresponding versions and a list of flag fields.
Of the three, only the list of names is required to have data in every
element. The other two lists will have the same number of elements, however.
The version values are only applied when the corresponding name refers to
another RPM package.

When a version is specified, the corresponding package may need to be
logically equal to, less than (older than) or greater (newer) than the
version as specified. This is signified in the corresponding flags field
for the triple. The flags documented later (see L<"Dependancy Sense Flags">)
can be used to determine the specific relationship.

=head2 The Trigger Specifications

The concept of trigger scripts was added into RPM from version 3.0 onwards.
It provides a powerful and flexible (if delicate and tricky) mechanism by
which packages may be sensitive to the installation, un-installation or
upgrade of other packages. In C<RPM::Header> terms, triggers are managed
through a combination of seven different header tags.

Firstly, the tags C<RPMTAG_TRIGGERSCRIPTS> and C<RPMTAG_TRIGGERSCRIPTPROG>
behave in the same fashion as similar tags for other script specifications.
All the triggers are stored on the B<TRIGGERSCRIPTS> tag, with each script
stored as one contiguous string. The B<TRIGGERSCRIPTPROG> array will specify
the program (and optional additional arguments) if the program is anything
other than C</bin/sh> (with no arguments).

The C<RPMTAG_TRIGGERNAME> and C<RPMTAG_TRIGGERVERSION> lists are used to
specify the packages that a given trigger is sensitive to. The name refers
to the package name (as RPM knows it to be), while the version (if specified)
further narrows the dependancy. The C<RPMTAG_TRIGGERCONDS> tag appears to be
present for future use, but the C<RPMTAG_TRIGGERFLAGS> is used as similarly-
named tags are for other script specifiers. In addition to the usual relative
comparison flags, these will also have some trigger-specific flags that
identify the trigger as being attached to an install, un-install or upgrade.
See L<"Dependancy Sense Flags">.

Lastly, the C<RPMTAG_TRIGGERINDEX> list is used to associate a given trigger
entry (in the B<TRIGGERNAME> list) with a particular script from the
B<TRIGGERSCRIPTS> list. This is to optimize storage, as the likelihood exists
that a given script may be re-used for more than one trigger.

The tags C<RPMTAG_TRIGGERNAME>, C<RPMTAG_TRIGGERVERSION>,
C<RPMTAG_TRIGGERFLAGS> and C<RPMTAG_TRIGGERINDEX> must all have the same
number of elements.

=head2 Dependancy Sense Flags

The following values may be imported via the tag B<:rpmsense>, and are
used with the flags values from various triple-tag combinations, to establish
the nature of the requirement relationship. In the paragraphs below, The C<*>
refers to any of B<REQUIRE>, B<OBSOLETE>, B<PROVIDE> or B<CONFLICT>. The
trigger-related flags have different uses than the rest of the B<:rpmsense>
set, though they may also make use of the flags for version comparison.

=over

=item RPMSENSE_SENSEMASK

This is a mask that, when applied to a value from B<RPMTAG_*FLAGS>,
masks out all bits except for the following three values:

=item RPMSENSE_EQUAL

=item RPMSENSE_GREATER

=item RPMSENSE_LESS

These values are used to check the corresponding entries from
B<RPMTAG_*NAME> and B<RPMTAG_*VERSION>, and specify whether
the existing file should be of a version equal to, greater than or less than
the version specified. More than one flag may be present.

=item RPMSENSE_PREREQ

The corresponding item from B<RPMTAG_*NAME> is a simple pre-requisite,
generally without specific version checking.

=item RPMSENSE_TRIGGER

A mask value that will isolate the trigger flags below from any other data
in the flag field.

=item RPMSENSE_TRIGGERIN

The corresponding trigger is an installation trigger.

=item RPMSENSE_TRIGGERUN

The corresponding trigger is an uninstallation trigger.

=item RPMSENSE_TRIGGERPOSTUN

The corresponding trigger is a post-uninstallation trigger.

=back

=head2 Header Data Types

The following symbols may be imported via the tag B<:rpmtype>, and represent
the different types of which the various header tags (described above) may
return data:

=over

=item RPM_NULL_TYPE

This is used internally by the C-level B<rpm> library.

=item RPM_CHAR_TYPE

This type represents single-character data.

=item RPM_INT8_TYPE

All items of this type are 8-bit integers.

=item RPM_INT16_TYPE

This type represents 16-bit integers.

=item RPM_INT32_TYPE

This type represents 32-bit integers.

=item RPM_BIN_TYPE

Data of this type represents a chunk of binary data without any further
decoding or translation. It is stored as a string in Perl terms, and the
C<length> keyword should return the size of the chunk.

=item RPM_STRING_TYPE

=item RPM_STRING_ARRAY_TYPE

=item RPM_I18NSTRING_TYPE

These data types represent strings of text. Each are stored and treated the
same internally by Perl.

=back

=head2 Error Codes

The following symbols may be imported via the tag B<:rpmerr>. They represent
the set of pre-defined error conditions that the B<rpm> system anticipates
as possibly occuring:

=over

=item RPMERR_BADARG

This is the most common error type used within the Perl RPM bindings. It is
used here to indicate bad or missing data in method calls.

=item RPMERR_BADDEV

Signaled when a file in the contents list is a bad or unknown device type.

=item RPMERR_BADFILENAME

This error signifies that RPM was unable to generate a filename, or that a
filename that RPM tried to use led to an error.

=item RPMERR_BADMAGIC

Signaled whenever an attempt to read the lead-in of the header (the "file magic"
information) fails. May be due either to bad data in that part, or an I/O
failure in reading the data itself.

=item RPMERR_BADRELOCATE

An error with the relocation specifications in the spec file.

=item RPMERR_BADSIGTYPE

Signals that an older, obsoleted style of signature was detected.

=item RPMERR_BADSPEC

General errors in the parsing or processing of the spec file.

=item RPMERR_CHOWN

An error occured in using the B<chown> system call.

=item RPMERR_CPIO

Errors that may occur when using B<cpio> to either package or unpack the source.

=item RPMERR_CREATE

This is signaled when RPM cannot create a directory or file.

=item RPMERR_DBCORRUPT

Signaled for consistency errors found in the database.

=item RPMERR_DBGETINDEX

This error represents a failure to read a requested header record from the
database.

=item RPMERR_DBOPEN

An error when opening some component of the database.

=item RPMERR_DBPUTINDEX

This error signals a failure to either store or remove a specified entry into
(or from) the database.

=item RPMERR_EXEC

An error occured when executing a sub-command (such as B<pgp>).

=item RPMERR_FILECONFLICT

A file conflict (not otherwise caught or handled by B<rpm> itself) was detected.

=item RPMERR_FLOCK

A failure to obtain a lock on the database. When the RPM library opens the
database it places an exclusive lock on it. As such, there cannot be two
processes (or two B<RPM::Database> instances) accessing the database at one
time.

=item RPMERR_FORK

An error occured when RPM tried to fork a child process.

=item RPMERR_GDBMOPEN

An error occured when trying to open a GDBM (GNU DBM) database.

=item RPMERR_GDBMREAD

An error occured when trying to read from a GDBM database.

=item RPMERR_GDBMWRITE

An error occured when trying to write to a GDBM database.

=item RPMERR_GZIP

An error occured with the B<gzip> program.

=item RPMERR_INTERNAL

This is used to signal internal errors from within the RPM library. Odds are,
if your program sees this error, you should exit as cleanly and quickly as
possible.

=item RPMERR_LDD

An error occurred with the B<ldd> program.

=item RPMERR_MKDIR

An error code was returned from the C<mkdir> system call.

=item RPMERR_MTAB

An error occured when trying to determine file system information from the
system C<mtab> file.

=item RPMERR_NEWPACKAGE

An attempt was made to create a new package with a specification of an RPM
version older (less) than 3.

=item RPMERR_NOCREATEDB

An attempt was made to create the database when one already exists.

=item RPMERR_NOGROUP

A group specified for file group-ownership was not found in the list of groups
on the system. The group C<root> will be used instead.

=item RPMERR_NORELOCATE

An attempt was made to relocate a package that is not relocatable.

=item RPMERR_NOSPACE

An attempt to write a package file failed for lack of available disk space.

=item RPMERR_NOSPEC

Am unpack operation on a source RPM failed to produce a spec file.

=item RPMERR_NOTSRPM

An operation was requested that can only be performed on a source RPM, but the
specified package was a binary (or C<noarch>) RPM.

=item RPMERR_NOUSER

A specified user (for file ownership) does not exist, and C<root> will be used
in its place. See B<RPMERR_NOGROUP>.

=item RPMERR_OLDDB

An old-format database is present.

=item RPMERR_OLDDBCORRUPT

An old-format database being read (for conversion) was found to be corrupt.

=item RPMERR_OLDDBMISSING

A request to convert an old-format database found that there was no such
database present.

=item RPMERR_OLDPACKAGE

An old-format package was detected.

=item RPMERR_PKGINSTALLED

A package requested for install is already installed on the system.

=item RPMERR_READERROR

An error occurred while reading data.

=item RPMERR_RENAME

An error occured while renaming a file.

=item RPMERR_RMDIR

An attempted removal of a directory failed.

=item RPMERR_RPMRC

A parsing or format error in an RC (options) file occurred.

=item RPMERR_SCRIPT

An error occurred while executing a script.

=item RPMERR_SIGGEN

Some type of error occurred when generating a signature on the package.

=item RPMERR_STAT

There was a failure of some sort on a C<stat> system call.

=item RPMERR_UNKNOWNARCH

A requested architecture is unknown to RPM.

=item RPMERR_UNKNOWNOS

A requested operating system is unknown to RPM.

=item RPMERR_UNLINK

An error occurred with the C<unlink> system call.

=item RPMERR_UNMATCHEDIF

An C<%else> or C<%endif> directive was seen in the spec file, for which there
is no corresponding C<%if>.

=back

=head2 File-Verification Flags

The values in the B<RPMTAG_FILEVERIFYFLAGS> list defined in the header-tags
section earlier represent various combinations of the following values. These
tags may be imported via B<:rpmverify>.

=over

=item RPMVERIFY_ALL

A full mask that will isolate the valid flag-bits from the flag field.

=item RPMVERIFY_NONE

An empty mask that will not match any tested verification flags.

=item RPMVERIFY_FILESIZE

Test the file size against the value in the header.

=item RPMVERIFY_GROUP

Test the file group ID against the value it should have been set to.

=item RPMVERIFY_LINKTO

If the file was to be a symbolic link, check that it is set correctly.

=item RPMVERIFY_MD5

Check the MD5 checksum for the file.

=item RPMVERIFY_MODE

Verify the file mode against the value it was to be set to.

=item RPMVERIFY_MTIME

Check the file modification-time against that which it should have been set.

=item RPMVERIFY_RDEV

Check the device field of the inode, if relevant.

=item RPMVERIFY_USER

Check the user ID to which ownership was set.

=back

When the verification of a given file fails, the return value contains the
relevant bits from the values above, corresponding to what test(s) failed.
In addition, any of the following may be set to indicate a larger problem:

=over

=item RPMVERIFY_LSTATFAIL

The attempt to read the inode information via C<lstat()> was not successful.
This will guarantee that other bits in the return value are set, as well.

=item RPMVERIFY_READFAIL

The attempt to read the file or its data (for the sake of MD5, etc.) failed.

=item RPMVERIFY_READLINKFAIL

An attempt to do a C<readlink()> on the file, expected to be a symbolic link,
failed.

=back

=head2 File Specification Flags

The following tags may be imported via the B<:rpmfile> specifier. They are
used to express various characteristics of files in the archive, based on the
value from B<RPMTAG_FILEFLAGS> that corresponds to a given file.

=over

=item RPMFILE_CONFIG

Not documented yet.

=item RPMFILE_DOC

Not documented yet.

=item RPMFILE_DONOTUSE

Not documented yet.

=item RPMFILE_GHOST

Not documented yet.

=item RPMFILE_LICENSE

Not documented yet.

=item RPMFILE_MISSINGOK

Not documented yet.

=item RPMFILE_NOREPLACE

Not documented yet.

=item RPMFILE_README

Not documented yet.

=item RPMFILE_SPECFILE

Not documented yet.

=item RPMFILE_STATE_NETSHARED

Not documented yet.

=item RPMFILE_STATE_NORMAL

Not documented yet.

=item RPMFILE_STATE_NOTINSTALLED

Not documented yet.

=item RPMFILE_STATE_REPLACED

Not documented yet.

=back

=head2 Not Yet Defined

The following have not yet been categorized. They may, after further research
and development, be found to be un-needed by this package.

=over

=item ADD_SIGNATURE

Not documented yet.

=item CHECKSIG_GPG

Not documented yet.

=item CHECKSIG_MD5

Not documented yet.

=item CHECKSIG_PGP

Not documented yet.

=item INSTALL_HASH

Not documented yet.

=item INSTALL_LABEL

Not documented yet.

=item INSTALL_NODEPS

Not documented yet.

=item INSTALL_NOORDER

Not documented yet.

=item INSTALL_PERCENT

Not documented yet.

=item INSTALL_UPGRADE

Not documented yet.

=item QUERY_FOR_CONFIG

Not documented yet.

=item QUERY_FOR_DOCS

Not documented yet.

=item QUERY_FOR_DUMPFILES

Not documented yet.

=item QUERY_FOR_LIST

Not documented yet.

=item QUERY_FOR_STATE

Not documented yet.

=item RPMPROB_FILTER_DISKSPACE

Not documented yet.

=item RPMPROB_FILTER_FORCERELOCATE

Not documented yet.

=item RPMPROB_FILTER_IGNOREARCH

Not documented yet.

=item RPMPROB_FILTER_IGNOREOS

Not documented yet.

=item RPMPROB_FILTER_OLDPACKAGE

Not documented yet.

=item RPMPROB_FILTER_REPLACENEWFILES

Not documented yet.

=item RPMPROB_FILTER_REPLACEOLDFILES

Not documented yet.

=item RPMPROB_FILTER_REPLACEPKG

Not documented yet.

=item RPMSIGTAG_GPG

Not documented yet.

=item RPMSIGTAG_LEMD5_1

Not documented yet.

=item RPMSIGTAG_LEMD5_2

Not documented yet.

=item RPMSIGTAG_MD5

Not documented yet.

=item RPMSIGTAG_PGP

Not documented yet.

=item RPMSIGTAG_PGP5

Not documented yet.

=item RPMSIGTAG_SIZE

Not documented yet.

=item RPMSIG_BAD

Not documented yet.

=item RPMSIG_NOKEY

Not documented yet.

=item RPMSIG_NOTTRUSTED

Not documented yet.

=item RPMSIG_OK

Not documented yet.

=item RPMSIG_UNKNOWN

Not documented yet.

=item RPMTRANS_FLAG_ALLFILES

Not documented yet.

=item RPMTRANS_FLAG_BUILD_PROBS

Not documented yet.

=item RPMTRANS_FLAG_JUSTDB

Not documented yet.

=item RPMTRANS_FLAG_KEEPOBSOLETE

Not documented yet.

=item RPMTRANS_FLAG_NODOCS

Not documented yet.

=item RPMTRANS_FLAG_NOSCRIPTS

Not documented yet.

=item RPMTRANS_FLAG_NOTRIGGERS

Not documented yet.

=item RPMTRANS_FLAG_TEST

Not documented yet.

=item UNINSTALL_ALLMATCHES

Not documented yet.

=item UNINSTALL_NODEPS

Not documented yet.

=item VERIFY_DEPS

Not documented yet.

=item VERIFY_FILES

Not documented yet.

=item VERIFY_MD5

Not documented yet.

=item VERIFY_SCRIPT

Not documented yet.

=back

=head1 SEE ALSO

L<RPM>, L<perl>, L<rpm>

=head1 AUTHOR

Randy J. Ray <rjray@blackperl.com>

=cut
