I have found, through fairly extensive testing and bug reports, that if the
Perl you try to use when building and using the Perl-RPM package was built to
immediately link any database library, then the RPM code will be unable to
open the rpm databases.

This is due to the fact that Perl creates backwards-compatibility to the Perl 4
dbmopen()/dbmclose() facility through linking of a DBM implementation into the
base executable. Though I couldn't exactly determine why, even if the only
such linkage is Berkeley DB (libdb.so, the same library used by rpm) this
earlier linkage will cause calls to dbopen() made by rpm into db to fail.

At this time, I don't have a solution for this problem, except to build Perl
without any of those libraries linked in Perl itself. Note that this will NOT
prevent you from building any of DB_File, GDBM_File, or NDBM_File (as installed
software permits) as dynamic extensions. This is the way Perl is distributed
on Red Hat Linux systems. No database libraries are natively linked, but both
DB_File and GDBM_File are still available as normal.

Building Perl in this fashion is not difficult, but not particularly easy,
either. It is necessary to remove any of "-ldb", "-lgdbm" and/or "-lndbm"
from the libraries that Perl selects for linking (these are usually listed
immediately before the dyna-linking library, -ldl on my Linux machines). Once
these are removed, they cause Configure to choose not to include DB_File or
GDBM_File in the list of dynamic or static extensions. This is easier to
remedy, since one can use variable substitution to add them back to the end of
the list:

	$* DB_File GDBM_File

Most likely, this disables the dbmopen/close functionality, though I haven't
tested to see if it is still accessible. Most likely, you won't be using it,
at least not with this particular extension.

I apologize for this inconvenience, and hope to find a way around it in future
versions of Perl. If you use Perl installations in RPM form (from rpm.org or
from Red Hat), this shouldn't be a problem.

Randy J. Ray <rjray@blackperl.com>
Fri Jun  2 18:21:24 PDT 2000

