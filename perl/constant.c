/*
 * This file contains the constant() function that is used
 * by perl AutoLoader when looking up exported constant symbols
 *
 * $Id: constant.c,v 1.2 1999/07/14 18:23:16 gafton Exp $
 */

#include "EXTERN.h" 
#include "perl.h" 
#include "XSUB.h" 
 
#include <rpm/rpmio.h> 
#include <rpm/dbindex.h> 
#include <rpm/header.h> 
#include <popt.h> 
#include <rpm/rpmlib.h> 

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

double
constant(char *name, int arg)
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	break;
    case 'D':
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	if (strEQ(name, "QUERY_FOR_CONFIG"))
#ifdef QUERY_FOR_CONFIG
	    return QUERY_FOR_CONFIG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "QUERY_FOR_DOCS"))
#ifdef QUERY_FOR_DOCS
	    return QUERY_FOR_DOCS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "QUERY_FOR_DUMPFILES"))
#ifdef QUERY_FOR_DUMPFILES
	    return QUERY_FOR_DUMPFILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "QUERY_FOR_LIST"))
#ifdef QUERY_FOR_LIST
	    return QUERY_FOR_LIST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "QUERY_FOR_STATE"))
#ifdef QUERY_FOR_STATE
	    return QUERY_FOR_STATE;
#else
	    goto not_there;
#endif
	break;
    case 'R':
	if (strEQ(name, "RPMERR_BADARG"))
#ifdef RPMERR_BADARG
	    return RPMERR_BADARG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADDEV"))
#ifdef RPMERR_BADDEV
	    return RPMERR_BADDEV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADFILENAME"))
#ifdef RPMERR_BADFILENAME
	    return RPMERR_BADFILENAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADMAGIC"))
#ifdef RPMERR_BADMAGIC
	    return RPMERR_BADMAGIC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADRELOCATE"))
#ifdef RPMERR_BADRELOCATE
	    return RPMERR_BADRELOCATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADSIGTYPE"))
#ifdef RPMERR_BADSIGTYPE
	    return RPMERR_BADSIGTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_BADSPEC"))
#ifdef RPMERR_BADSPEC
	    return RPMERR_BADSPEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_CHOWN"))
#ifdef RPMERR_CHOWN
	    return RPMERR_CHOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_CPIO"))
#ifdef RPMERR_CPIO
	    return RPMERR_CPIO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_CREATE"))
#ifdef RPMERR_CREATE
	    return RPMERR_CREATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_DBCORRUPT"))
#ifdef RPMERR_DBCORRUPT
	    return RPMERR_DBCORRUPT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_DBGETINDEX"))
#ifdef RPMERR_DBGETINDEX
	    return RPMERR_DBGETINDEX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_DBOPEN"))
#ifdef RPMERR_DBOPEN
	    return RPMERR_DBOPEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_DBPUTINDEX"))
#ifdef RPMERR_DBPUTINDEX
	    return RPMERR_DBPUTINDEX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_EXEC"))
#ifdef RPMERR_EXEC
	    return RPMERR_EXEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_FILECONFLICT"))
#ifdef RPMERR_FILECONFLICT
	    return RPMERR_FILECONFLICT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_FLOCK"))
#ifdef RPMERR_FLOCK
	    return RPMERR_FLOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_FORK"))
#ifdef RPMERR_FORK
	    return RPMERR_FORK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_GDBMOPEN"))
#ifdef RPMERR_GDBMOPEN
	    return RPMERR_GDBMOPEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_GDBMREAD"))
#ifdef RPMERR_GDBMREAD
	    return RPMERR_GDBMREAD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_GDBMWRITE"))
#ifdef RPMERR_GDBMWRITE
	    return RPMERR_GDBMWRITE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_GZIP"))
#ifdef RPMERR_GZIP
	    return RPMERR_GZIP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_INTERNAL"))
#ifdef RPMERR_INTERNAL
	    return RPMERR_INTERNAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_LDD"))
#ifdef RPMERR_LDD
	    return RPMERR_LDD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_MKDIR"))
#ifdef RPMERR_MKDIR
	    return RPMERR_MKDIR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_MTAB"))
#ifdef RPMERR_MTAB
	    return RPMERR_MTAB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NEWPACKAGE"))
#ifdef RPMERR_NEWPACKAGE
	    return RPMERR_NEWPACKAGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOCREATEDB"))
#ifdef RPMERR_NOCREATEDB
	    return RPMERR_NOCREATEDB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOGROUP"))
#ifdef RPMERR_NOGROUP
	    return RPMERR_NOGROUP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NORELOCATE"))
#ifdef RPMERR_NORELOCATE
	    return RPMERR_NORELOCATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOSPACE"))
#ifdef RPMERR_NOSPACE
	    return RPMERR_NOSPACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOSPEC"))
#ifdef RPMERR_NOSPEC
	    return RPMERR_NOSPEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOTSRPM"))
#ifdef RPMERR_NOTSRPM
	    return RPMERR_NOTSRPM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_NOUSER"))
#ifdef RPMERR_NOUSER
	    return RPMERR_NOUSER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_OLDDB"))
#ifdef RPMERR_OLDDB
	    return RPMERR_OLDDB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_OLDDBCORRUPT"))
#ifdef RPMERR_OLDDBCORRUPT
	    return RPMERR_OLDDBCORRUPT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_OLDDBMISSING"))
#ifdef RPMERR_OLDDBMISSING
	    return RPMERR_OLDDBMISSING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_OLDPACKAGE"))
#ifdef RPMERR_OLDPACKAGE
	    return RPMERR_OLDPACKAGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_PKGINSTALLED"))
#ifdef RPMERR_PKGINSTALLED
	    return RPMERR_PKGINSTALLED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_READERROR"))
#ifdef RPMERR_READERROR
	    return RPMERR_READERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_RENAME"))
#ifdef RPMERR_RENAME
	    return RPMERR_RENAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_RMDIR"))
#ifdef RPMERR_RMDIR
	    return RPMERR_RMDIR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_RPMRC"))
#ifdef RPMERR_RPMRC
	    return RPMERR_RPMRC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_SCRIPT"))
#ifdef RPMERR_SCRIPT
	    return RPMERR_SCRIPT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_SIGGEN"))
#ifdef RPMERR_SIGGEN
	    return RPMERR_SIGGEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_STAT"))
#ifdef RPMERR_STAT
	    return RPMERR_STAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_UNKNOWNARCH"))
#ifdef RPMERR_UNKNOWNARCH
	    return RPMERR_UNKNOWNARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_UNKNOWNOS"))
#ifdef RPMERR_UNKNOWNOS
	    return RPMERR_UNKNOWNOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_UNLINK"))
#ifdef RPMERR_UNLINK
	    return RPMERR_UNLINK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMERR_UNMATCHEDIF"))
#ifdef RPMERR_UNMATCHEDIF
	    return RPMERR_UNMATCHEDIF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_CONFIG"))
#ifdef RPMFILE_CONFIG
	    return RPMFILE_CONFIG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_DOC"))
#ifdef RPMFILE_DOC
	    return RPMFILE_DOC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_DONOTUSE"))
#ifdef RPMFILE_DONOTUSE
	    return RPMFILE_DONOTUSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_GHOST"))
#ifdef RPMFILE_GHOST
	    return RPMFILE_GHOST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_LICENSE"))
#ifdef RPMFILE_LICENSE
	    return RPMFILE_LICENSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_MISSINGOK"))
#ifdef RPMFILE_MISSINGOK
	    return RPMFILE_MISSINGOK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_NOREPLACE"))
#ifdef RPMFILE_NOREPLACE
	    return RPMFILE_NOREPLACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_README"))
#ifdef RPMFILE_README
	    return RPMFILE_README;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_SPECFILE"))
#ifdef RPMFILE_SPECFILE
	    return RPMFILE_SPECFILE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_STATE_NETSHARED"))
#ifdef RPMFILE_STATE_NETSHARED
	    return RPMFILE_STATE_NETSHARED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_STATE_NORMAL"))
#ifdef RPMFILE_STATE_NORMAL
	    return RPMFILE_STATE_NORMAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_STATE_NOTINSTALLED"))
#ifdef RPMFILE_STATE_NOTINSTALLED
	    return RPMFILE_STATE_NOTINSTALLED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMFILE_STATE_REPLACED"))
#ifdef RPMFILE_STATE_REPLACED
	    return RPMFILE_STATE_REPLACED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_BINARY"))
#ifdef RPMLEAD_BINARY
	    return RPMLEAD_BINARY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_MAGIC0"))
#ifdef RPMLEAD_MAGIC0
	    return RPMLEAD_MAGIC0;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_MAGIC1"))
#ifdef RPMLEAD_MAGIC1
	    return RPMLEAD_MAGIC1;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_MAGIC2"))
#ifdef RPMLEAD_MAGIC2
	    return RPMLEAD_MAGIC2;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_MAGIC3"))
#ifdef RPMLEAD_MAGIC3
	    return RPMLEAD_MAGIC3;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_SIZE"))
#ifdef RPMLEAD_SIZE
	    return RPMLEAD_SIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMLEAD_SOURCE"))
#ifdef RPMLEAD_SOURCE
	    return RPMLEAD_SOURCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_ALTNAME"))
#ifdef RPMMESS_ALTNAME
	    return RPMMESS_ALTNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_BACKUP"))
#ifdef RPMMESS_BACKUP
	    return RPMMESS_BACKUP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_DEBUG"))
#ifdef RPMMESS_DEBUG
	    return RPMMESS_DEBUG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_ERROR"))
#ifdef RPMMESS_ERROR
	    return RPMMESS_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_FATALERROR"))
#ifdef RPMMESS_FATALERROR
	    return RPMMESS_FATALERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_NORMAL"))
#ifdef RPMMESS_NORMAL
	    return RPMMESS_NORMAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_PREREQLOOP"))
#ifdef RPMMESS_PREREQLOOP
	    return RPMMESS_PREREQLOOP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_QUIET"))
#ifdef RPMMESS_QUIET
	    return RPMMESS_QUIET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_VERBOSE"))
#ifdef RPMMESS_VERBOSE
	    return RPMMESS_VERBOSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMMESS_WARNING"))
#ifdef RPMMESS_WARNING
	    return RPMMESS_WARNING;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_DISKSPACE"))
#ifdef RPMPROB_FILTER_DISKSPACE
	    return RPMPROB_FILTER_DISKSPACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_FORCERELOCATE"))
#ifdef RPMPROB_FILTER_FORCERELOCATE
	    return RPMPROB_FILTER_FORCERELOCATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_IGNOREARCH"))
#ifdef RPMPROB_FILTER_IGNOREARCH
	    return RPMPROB_FILTER_IGNOREARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_IGNOREOS"))
#ifdef RPMPROB_FILTER_IGNOREOS
	    return RPMPROB_FILTER_IGNOREOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_OLDPACKAGE"))
#ifdef RPMPROB_FILTER_OLDPACKAGE
	    return RPMPROB_FILTER_OLDPACKAGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_REPLACENEWFILES"))
#ifdef RPMPROB_FILTER_REPLACENEWFILES
	    return RPMPROB_FILTER_REPLACENEWFILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_REPLACEOLDFILES"))
#ifdef RPMPROB_FILTER_REPLACEOLDFILES
	    return RPMPROB_FILTER_REPLACEOLDFILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMPROB_FILTER_REPLACEPKG"))
#ifdef RPMPROB_FILTER_REPLACEPKG
	    return RPMPROB_FILTER_REPLACEPKG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_ANY"))
#ifdef RPMSENSE_ANY
	    return RPMSENSE_ANY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_CONFLICTS"))
#ifdef RPMSENSE_CONFLICTS
	    return RPMSENSE_CONFLICTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_EQUAL"))
#ifdef RPMSENSE_EQUAL
	    return RPMSENSE_EQUAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_GREATER"))
#ifdef RPMSENSE_GREATER
	    return RPMSENSE_GREATER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_LESS"))
#ifdef RPMSENSE_LESS
	    return RPMSENSE_LESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_OBSOLETES"))
#ifdef RPMSENSE_OBSOLETES
	    return RPMSENSE_OBSOLETES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_PREREQ"))
#ifdef RPMSENSE_PREREQ
	    return RPMSENSE_PREREQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_PROVIDES"))
#ifdef RPMSENSE_PROVIDES
	    return RPMSENSE_PROVIDES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_SENSEMASK"))
#ifdef RPMSENSE_SENSEMASK
	    return RPMSENSE_SENSEMASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_SERIAL"))
#ifdef RPMSENSE_SERIAL
	    return RPMSENSE_SERIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_TRIGGER"))
#ifdef RPMSENSE_TRIGGER
	    return RPMSENSE_TRIGGER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_TRIGGERIN"))
#ifdef RPMSENSE_TRIGGERIN
	    return RPMSENSE_TRIGGERIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_TRIGGERPOSTUN"))
#ifdef RPMSENSE_TRIGGERPOSTUN
	    return RPMSENSE_TRIGGERPOSTUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSENSE_TRIGGERUN"))
#ifdef RPMSENSE_TRIGGERUN
	    return RPMSENSE_TRIGGERUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_GPG"))
#ifdef RPMSIGTAG_GPG
	    return RPMSIGTAG_GPG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_LEMD5_1"))
#ifdef RPMSIGTAG_LEMD5_1
	    return RPMSIGTAG_LEMD5_1;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_LEMD5_2"))
#ifdef RPMSIGTAG_LEMD5_2
	    return RPMSIGTAG_LEMD5_2;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_MD5"))
#ifdef RPMSIGTAG_MD5
	    return RPMSIGTAG_MD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_PGP"))
#ifdef RPMSIGTAG_PGP
	    return RPMSIGTAG_PGP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_PGP5"))
#ifdef RPMSIGTAG_PGP5
	    return RPMSIGTAG_PGP5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIGTAG_SIZE"))
#ifdef RPMSIGTAG_SIZE
	    return RPMSIGTAG_SIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIG_BAD"))
#ifdef RPMSIG_BAD
	    return RPMSIG_BAD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIG_NOKEY"))
#ifdef RPMSIG_NOKEY
	    return RPMSIG_NOKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIG_NOTTRUSTED"))
#ifdef RPMSIG_NOTTRUSTED
	    return RPMSIG_NOTTRUSTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIG_OK"))
#ifdef RPMSIG_OK
	    return RPMSIG_OK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMSIG_UNKNOWN"))
#ifdef RPMSIG_UNKNOWN
	    return RPMSIG_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_ARCH"))
#ifdef RPMTAG_ARCH
	    return RPMTAG_ARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_ARCHIVESIZE"))
#ifdef RPMTAG_ARCHIVESIZE
	    return RPMTAG_ARCHIVESIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_AUTOPROV"))
#ifdef RPMTAG_AUTOPROV
	    return RPMTAG_AUTOPROV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_AUTOREQ"))
#ifdef RPMTAG_AUTOREQ
	    return RPMTAG_AUTOREQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_AUTOREQPROV"))
#ifdef RPMTAG_AUTOREQPROV
	    return RPMTAG_AUTOREQPROV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BROKENMD5"))
#ifdef RPMTAG_BROKENMD5
	    return RPMTAG_BROKENMD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDARCHS"))
#ifdef RPMTAG_BUILDARCHS
	    return RPMTAG_BUILDARCHS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDCONFLICTS"))
#ifdef RPMTAG_BUILDCONFLICTS
	    return RPMTAG_BUILDCONFLICTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDHOST"))
#ifdef RPMTAG_BUILDHOST
	    return RPMTAG_BUILDHOST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDMACROS"))
#ifdef RPMTAG_BUILDMACROS
	    return RPMTAG_BUILDMACROS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDPREREQ"))
#ifdef RPMTAG_BUILDPREREQ
	    return RPMTAG_BUILDPREREQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDREQUIRES"))
#ifdef RPMTAG_BUILDREQUIRES
	    return RPMTAG_BUILDREQUIRES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDROOT"))
#ifdef RPMTAG_BUILDROOT
	    return RPMTAG_BUILDROOT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_BUILDTIME"))
#ifdef RPMTAG_BUILDTIME
	    return RPMTAG_BUILDTIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CAPABILITY"))
#ifdef RPMTAG_CAPABILITY
	    return RPMTAG_CAPABILITY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CHANGELOG"))
#ifdef RPMTAG_CHANGELOG
	    return RPMTAG_CHANGELOG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CHANGELOGNAME"))
#ifdef RPMTAG_CHANGELOGNAME
	    return RPMTAG_CHANGELOGNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CHANGELOGTEXT"))
#ifdef RPMTAG_CHANGELOGTEXT
	    return RPMTAG_CHANGELOGTEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CHANGELOGTIME"))
#ifdef RPMTAG_CHANGELOGTIME
	    return RPMTAG_CHANGELOGTIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CONFLICTFLAGS"))
#ifdef RPMTAG_CONFLICTFLAGS
	    return RPMTAG_CONFLICTFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CONFLICTNAME"))
#ifdef RPMTAG_CONFLICTNAME
	    return RPMTAG_CONFLICTNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_CONFLICTVERSION"))
#ifdef RPMTAG_CONFLICTVERSION
	    return RPMTAG_CONFLICTVERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_COOKIE"))
#ifdef RPMTAG_COOKIE
	    return RPMTAG_COOKIE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_COPYRIGHT"))
#ifdef RPMTAG_COPYRIGHT
	    return RPMTAG_COPYRIGHT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_DEFAULTPREFIX"))
#ifdef RPMTAG_DEFAULTPREFIX
	    return RPMTAG_DEFAULTPREFIX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_DESCRIPTION"))
#ifdef RPMTAG_DESCRIPTION
	    return RPMTAG_DESCRIPTION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_DISTRIBUTION"))
#ifdef RPMTAG_DISTRIBUTION
	    return RPMTAG_DISTRIBUTION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_DOCDIR"))
#ifdef RPMTAG_DOCDIR
	    return RPMTAG_DOCDIR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EPOCH"))
#ifdef RPMTAG_EPOCH
	    return RPMTAG_EPOCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUDE"))
#ifdef RPMTAG_EXCLUDE
	    return RPMTAG_EXCLUDE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUDEARCH"))
#ifdef RPMTAG_EXCLUDEARCH
	    return RPMTAG_EXCLUDEARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUDEOS"))
#ifdef RPMTAG_EXCLUDEOS
	    return RPMTAG_EXCLUDEOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUSIVE"))
#ifdef RPMTAG_EXCLUSIVE
	    return RPMTAG_EXCLUSIVE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUSIVEARCH"))
#ifdef RPMTAG_EXCLUSIVEARCH
	    return RPMTAG_EXCLUSIVEARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXCLUSIVEOS"))
#ifdef RPMTAG_EXCLUSIVEOS
	    return RPMTAG_EXCLUSIVEOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_EXTERNAL_TAG"))
#ifdef RPMTAG_EXTERNAL_TAG
	    return RPMTAG_EXTERNAL_TAG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEDEVICES"))
#ifdef RPMTAG_FILEDEVICES
	    return RPMTAG_FILEDEVICES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEFLAGS"))
#ifdef RPMTAG_FILEFLAGS
	    return RPMTAG_FILEFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEGIDS"))
#ifdef RPMTAG_FILEGIDS
	    return RPMTAG_FILEGIDS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEGROUPNAME"))
#ifdef RPMTAG_FILEGROUPNAME
	    return RPMTAG_FILEGROUPNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEINODES"))
#ifdef RPMTAG_FILEINODES
	    return RPMTAG_FILEINODES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILELANGS"))
#ifdef RPMTAG_FILELANGS
	    return RPMTAG_FILELANGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILELINKTOS"))
#ifdef RPMTAG_FILELINKTOS
	    return RPMTAG_FILELINKTOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEMD5S"))
#ifdef RPMTAG_FILEMD5S
	    return RPMTAG_FILEMD5S;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEMODES"))
#ifdef RPMTAG_FILEMODES
	    return RPMTAG_FILEMODES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEMTIMES"))
#ifdef RPMTAG_FILEMTIMES
	    return RPMTAG_FILEMTIMES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILENAMES"))
#ifdef RPMTAG_FILENAMES
	    return RPMTAG_FILENAMES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILERDEVS"))
#ifdef RPMTAG_FILERDEVS
	    return RPMTAG_FILERDEVS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILESIZES"))
#ifdef RPMTAG_FILESIZES
	    return RPMTAG_FILESIZES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILESTATES"))
#ifdef RPMTAG_FILESTATES
	    return RPMTAG_FILESTATES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEUIDS"))
#ifdef RPMTAG_FILEUIDS
	    return RPMTAG_FILEUIDS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEUSERNAME"))
#ifdef RPMTAG_FILEUSERNAME
	    return RPMTAG_FILEUSERNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_FILEVERIFYFLAGS"))
#ifdef RPMTAG_FILEVERIFYFLAGS
	    return RPMTAG_FILEVERIFYFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_GIF"))
#ifdef RPMTAG_GIF
	    return RPMTAG_GIF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_GROUP"))
#ifdef RPMTAG_GROUP
	    return RPMTAG_GROUP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_ICON"))
#ifdef RPMTAG_ICON
	    return RPMTAG_ICON;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_INSTALLPREFIX"))
#ifdef RPMTAG_INSTALLPREFIX
	    return RPMTAG_INSTALLPREFIX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_INSTALLTIME"))
#ifdef RPMTAG_INSTALLTIME
	    return RPMTAG_INSTALLTIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_INSTPREFIXES"))
#ifdef RPMTAG_INSTPREFIXES
	    return RPMTAG_INSTPREFIXES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_LICENSE"))
#ifdef RPMTAG_LICENSE
	    return RPMTAG_LICENSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_NAME"))
#ifdef RPMTAG_NAME
	    return RPMTAG_NAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_NOPATCH"))
#ifdef RPMTAG_NOPATCH
	    return RPMTAG_NOPATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_NOSOURCE"))
#ifdef RPMTAG_NOSOURCE
	    return RPMTAG_NOSOURCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_OBSOLETES"))
#ifdef RPMTAG_OBSOLETES
	    return RPMTAG_OBSOLETES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_ORIGFILENAMES"))
#ifdef RPMTAG_ORIGFILENAMES
	    return RPMTAG_ORIGFILENAMES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_OS"))
#ifdef RPMTAG_OS
	    return RPMTAG_OS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PACKAGER"))
#ifdef RPMTAG_PACKAGER
	    return RPMTAG_PACKAGER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PATCH"))
#ifdef RPMTAG_PATCH
	    return RPMTAG_PATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_POSTIN"))
#ifdef RPMTAG_POSTIN
	    return RPMTAG_POSTIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_POSTINPROG"))
#ifdef RPMTAG_POSTINPROG
	    return RPMTAG_POSTINPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_POSTUN"))
#ifdef RPMTAG_POSTUN
	    return RPMTAG_POSTUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_POSTUNPROG"))
#ifdef RPMTAG_POSTUNPROG
	    return RPMTAG_POSTUNPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREFIXES"))
#ifdef RPMTAG_PREFIXES
	    return RPMTAG_PREFIXES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREIN"))
#ifdef RPMTAG_PREIN
	    return RPMTAG_PREIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREINPROG"))
#ifdef RPMTAG_PREINPROG
	    return RPMTAG_PREINPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREREQ"))
#ifdef RPMTAG_PREREQ
	    return RPMTAG_PREREQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREUN"))
#ifdef RPMTAG_PREUN
	    return RPMTAG_PREUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PREUNPROG"))
#ifdef RPMTAG_PREUNPROG
	    return RPMTAG_PREUNPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_PROVIDES"))
#ifdef RPMTAG_PROVIDES
	    return RPMTAG_PROVIDES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_RELEASE"))
#ifdef RPMTAG_RELEASE
	    return RPMTAG_RELEASE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_REQUIREFLAGS"))
#ifdef RPMTAG_REQUIREFLAGS
	    return RPMTAG_REQUIREFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_REQUIRENAME"))
#ifdef RPMTAG_REQUIRENAME
	    return RPMTAG_REQUIRENAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_REQUIREVERSION"))
#ifdef RPMTAG_REQUIREVERSION
	    return RPMTAG_REQUIREVERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_ROOT"))
#ifdef RPMTAG_ROOT
	    return RPMTAG_ROOT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_RPMVERSION"))
#ifdef RPMTAG_RPMVERSION
	    return RPMTAG_RPMVERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SERIAL"))
#ifdef RPMTAG_SERIAL
	    return RPMTAG_SERIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SIZE"))
#ifdef RPMTAG_SIZE
	    return RPMTAG_SIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SOURCE"))
#ifdef RPMTAG_SOURCE
	    return RPMTAG_SOURCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SOURCEPACKAGE"))
#ifdef RPMTAG_SOURCEPACKAGE
	    return RPMTAG_SOURCEPACKAGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SOURCERPM"))
#ifdef RPMTAG_SOURCERPM
	    return RPMTAG_SOURCERPM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_SUMMARY"))
#ifdef RPMTAG_SUMMARY
	    return RPMTAG_SUMMARY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERFLAGS"))
#ifdef RPMTAG_TRIGGERFLAGS
	    return RPMTAG_TRIGGERFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERIN"))
#ifdef RPMTAG_TRIGGERIN
	    return RPMTAG_TRIGGERIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERINDEX"))
#ifdef RPMTAG_TRIGGERINDEX
	    return RPMTAG_TRIGGERINDEX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERNAME"))
#ifdef RPMTAG_TRIGGERNAME
	    return RPMTAG_TRIGGERNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERPOSTUN"))
#ifdef RPMTAG_TRIGGERPOSTUN
	    return RPMTAG_TRIGGERPOSTUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERSCRIPTPROG"))
#ifdef RPMTAG_TRIGGERSCRIPTPROG
	    return RPMTAG_TRIGGERSCRIPTPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERSCRIPTS"))
#ifdef RPMTAG_TRIGGERSCRIPTS
	    return RPMTAG_TRIGGERSCRIPTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERUN"))
#ifdef RPMTAG_TRIGGERUN
	    return RPMTAG_TRIGGERUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_TRIGGERVERSION"))
#ifdef RPMTAG_TRIGGERVERSION
	    return RPMTAG_TRIGGERVERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_URL"))
#ifdef RPMTAG_URL
	    return RPMTAG_URL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_VENDOR"))
#ifdef RPMTAG_VENDOR
	    return RPMTAG_VENDOR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_VERIFYSCRIPT"))
#ifdef RPMTAG_VERIFYSCRIPT
	    return RPMTAG_VERIFYSCRIPT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_VERIFYSCRIPTPROG"))
#ifdef RPMTAG_VERIFYSCRIPTPROG
	    return RPMTAG_VERIFYSCRIPTPROG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_VERSION"))
#ifdef RPMTAG_VERSION
	    return RPMTAG_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTAG_XPM"))
#ifdef RPMTAG_XPM
	    return RPMTAG_XPM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_ALLFILES"))
#ifdef RPMTRANS_FLAG_ALLFILES
	    return RPMTRANS_FLAG_ALLFILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_BUILD_PROBS"))
#ifdef RPMTRANS_FLAG_BUILD_PROBS
	    return RPMTRANS_FLAG_BUILD_PROBS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_JUSTDB"))
#ifdef RPMTRANS_FLAG_JUSTDB
	    return RPMTRANS_FLAG_JUSTDB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_KEEPOBSOLETE"))
#ifdef RPMTRANS_FLAG_KEEPOBSOLETE
	    return RPMTRANS_FLAG_KEEPOBSOLETE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_NODOCS"))
#ifdef RPMTRANS_FLAG_NODOCS
	    return RPMTRANS_FLAG_NODOCS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_NOSCRIPTS"))
#ifdef RPMTRANS_FLAG_NOSCRIPTS
	    return RPMTRANS_FLAG_NOSCRIPTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_NOTRIGGERS"))
#ifdef RPMTRANS_FLAG_NOTRIGGERS
	    return RPMTRANS_FLAG_NOTRIGGERS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMTRANS_FLAG_TEST"))
#ifdef RPMTRANS_FLAG_TEST
	    return RPMTRANS_FLAG_TEST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVAR_INCLUDE"))
#ifdef RPMVAR_INCLUDE
	    return RPMVAR_INCLUDE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVAR_MACROFILES"))
#ifdef RPMVAR_MACROFILES
	    return RPMVAR_MACROFILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVAR_NUM"))
#ifdef RPMVAR_NUM
	    return RPMVAR_NUM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVAR_OPTFLAGS"))
#ifdef RPMVAR_OPTFLAGS
	    return RPMVAR_OPTFLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVAR_PROVIDES"))
#ifdef RPMVAR_PROVIDES
	    return RPMVAR_PROVIDES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_ALL"))
#ifdef RPMVERIFY_ALL
	    return RPMVERIFY_ALL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_FILESIZE"))
#ifdef RPMVERIFY_FILESIZE
	    return RPMVERIFY_FILESIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_GROUP"))
#ifdef RPMVERIFY_GROUP
	    return RPMVERIFY_GROUP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_LINKTO"))
#ifdef RPMVERIFY_LINKTO
	    return RPMVERIFY_LINKTO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_LSTATFAIL"))
#ifdef RPMVERIFY_LSTATFAIL
	    return RPMVERIFY_LSTATFAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_MD5"))
#ifdef RPMVERIFY_MD5
	    return RPMVERIFY_MD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_MODE"))
#ifdef RPMVERIFY_MODE
	    return RPMVERIFY_MODE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_MTIME"))
#ifdef RPMVERIFY_MTIME
	    return RPMVERIFY_MTIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_NONE"))
#ifdef RPMVERIFY_NONE
	    return RPMVERIFY_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_RDEV"))
#ifdef RPMVERIFY_RDEV
	    return RPMVERIFY_RDEV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_READFAIL"))
#ifdef RPMVERIFY_READFAIL
	    return RPMVERIFY_READFAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_READLINKFAIL"))
#ifdef RPMVERIFY_READLINKFAIL
	    return RPMVERIFY_READLINKFAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPMVERIFY_USER"))
#ifdef RPMVERIFY_USER
	    return RPMVERIFY_USER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPM_MACHTABLE_BUILDARCH"))
#ifdef RPM_MACHTABLE_BUILDARCH
	    return RPM_MACHTABLE_BUILDARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPM_MACHTABLE_BUILDOS"))
#ifdef RPM_MACHTABLE_BUILDOS
	    return RPM_MACHTABLE_BUILDOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPM_MACHTABLE_COUNT"))
#ifdef RPM_MACHTABLE_COUNT
	    return RPM_MACHTABLE_COUNT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPM_MACHTABLE_INSTARCH"))
#ifdef RPM_MACHTABLE_INSTARCH
	    return RPM_MACHTABLE_INSTARCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RPM_MACHTABLE_INSTOS"))
#ifdef RPM_MACHTABLE_INSTOS
	    return RPM_MACHTABLE_INSTOS;
#else
	    goto not_there;
#endif
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	if (strEQ(name, "VERIFY_DEPS"))
#ifdef VERIFY_DEPS
	    return VERIFY_DEPS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VERIFY_FILES"))
#ifdef VERIFY_FILES
	    return VERIFY_FILES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VERIFY_MD5"))
#ifdef VERIFY_MD5
	    return VERIFY_MD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VERIFY_SCRIPT"))
#ifdef VERIFY_SCRIPT
	    return VERIFY_SCRIPT;
#else
	    goto not_there;
#endif
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

