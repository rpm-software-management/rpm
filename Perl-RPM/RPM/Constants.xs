#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Constants.xs,v 1.10 2001/05/12 11:40:27 rjray Exp $";

static int constant(pTHX_ char *name)
{
    errno = 0;

    switch (*name) {
    case 'A':
        break;
    case 'B':
        break;
    case 'C':
        if (strEQ(name, "CHECKSIG_GPG"))
            return CHECKSIG_GPG;
        if (strEQ(name, "CHECKSIG_MD5"))
            return CHECKSIG_MD5;
        if (strEQ(name, "CHECKSIG_PGP"))
            return CHECKSIG_PGP;
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
        if (strEQ(name, "INSTALL_HASH"))
            return INSTALL_HASH;
        if (strEQ(name, "INSTALL_LABEL"))
            return INSTALL_LABEL;
        if (strEQ(name, "INSTALL_NODEPS"))
            return INSTALL_NODEPS;
        if (strEQ(name, "INSTALL_NOORDER"))
            return INSTALL_NOORDER;
        if (strEQ(name, "INSTALL_PERCENT"))
            return INSTALL_PERCENT;
        if (strEQ(name, "INSTALL_UPGRADE"))
            return INSTALL_UPGRADE;
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
            return QUERY_FOR_CONFIG;
        if (strEQ(name, "QUERY_FOR_DOCS"))
            return QUERY_FOR_DOCS;
        if (strEQ(name, "QUERY_FOR_DUMPFILES"))
            return QUERY_FOR_DUMPFILES;
        if (strEQ(name, "QUERY_FOR_LIST"))
            return QUERY_FOR_LIST;
        if (strEQ(name, "QUERY_FOR_STATE"))
            return QUERY_FOR_STATE;
        break;
    case 'R':
        if (strEQ(name, "RPM_NULL_TYPE"))
            return RPM_NULL_TYPE;
        if (strEQ(name, "RPM_CHAR_TYPE"))
            return RPM_CHAR_TYPE;
        if (strEQ(name, "RPM_INT8_TYPE"))
            return RPM_INT8_TYPE;
        if (strEQ(name, "RPM_INT16_TYPE"))
            return RPM_INT16_TYPE;
        if (strEQ(name, "RPM_INT32_TYPE"))
            return RPM_INT32_TYPE;
        if (strEQ(name, "RPM_STRING_TYPE"))
            return RPM_STRING_TYPE;
        if (strEQ(name, "RPM_BIN_TYPE"))
            return RPM_BIN_TYPE;
        if (strEQ(name, "RPM_STRING_ARRAY_TYPE"))
            return RPM_STRING_ARRAY_TYPE;
        if (strEQ(name, "RPM_I18NSTRING_TYPE"))
            return RPM_I18NSTRING_TYPE;
        if (strEQ(name, "RPMERR_BADARG"))
            return RPMERR_BADARG;
        if (strEQ(name, "RPMERR_BADDEV"))
            return RPMERR_BADDEV;
        if (strEQ(name, "RPMERR_BADFILENAME"))
            return RPMERR_BADFILENAME;
        if (strEQ(name, "RPMERR_BADMAGIC"))
            return RPMERR_BADMAGIC;
        if (strEQ(name, "RPMERR_BADRELOCATE"))
            return RPMERR_BADRELOCATE;
        if (strEQ(name, "RPMERR_BADSIGTYPE"))
            return RPMERR_BADSIGTYPE;
        if (strEQ(name, "RPMERR_BADSPEC"))
            return RPMERR_BADSPEC;
        if (strEQ(name, "RPMERR_CHOWN"))
            return RPMERR_CHOWN;
        if (strEQ(name, "RPMERR_CPIO"))
            return RPMERR_CPIO;
        if (strEQ(name, "RPMERR_CREATE"))
            return RPMERR_CREATE;
        if (strEQ(name, "RPMERR_DBCORRUPT"))
            return RPMERR_DBCORRUPT;
        if (strEQ(name, "RPMERR_DBGETINDEX"))
            return RPMERR_DBGETINDEX;
        if (strEQ(name, "RPMERR_DBOPEN"))
            return RPMERR_DBOPEN;
        if (strEQ(name, "RPMERR_DBPUTINDEX"))
            return RPMERR_DBPUTINDEX;
        if (strEQ(name, "RPMERR_EXEC"))
            return RPMERR_EXEC;
        if (strEQ(name, "RPMERR_FILECONFLICT"))
            return RPMERR_FILECONFLICT;
        if (strEQ(name, "RPMERR_FLOCK"))
            return RPMERR_FLOCK;
        if (strEQ(name, "RPMERR_FORK"))
            return RPMERR_FORK;
        if (strEQ(name, "RPMERR_GDBMOPEN"))
            return RPMERR_GDBMOPEN;
        if (strEQ(name, "RPMERR_GDBMREAD"))
            return RPMERR_GDBMREAD;
        if (strEQ(name, "RPMERR_GDBMWRITE"))
            return RPMERR_GDBMWRITE;
        if (strEQ(name, "RPMERR_GZIP"))
            return RPMERR_GZIP;
        if (strEQ(name, "RPMERR_INTERNAL"))
            return RPMERR_INTERNAL;
        if (strEQ(name, "RPMERR_LDD"))
            return RPMERR_LDD;
        if (strEQ(name, "RPMERR_MKDIR"))
            return RPMERR_MKDIR;
        if (strEQ(name, "RPMERR_MTAB"))
            return RPMERR_MTAB;
        if (strEQ(name, "RPMERR_NEWPACKAGE"))
            return RPMERR_NEWPACKAGE;
        if (strEQ(name, "RPMERR_NOCREATEDB"))
            return RPMERR_NOCREATEDB;
        if (strEQ(name, "RPMERR_NOGROUP"))
            return RPMERR_NOGROUP;
        if (strEQ(name, "RPMERR_NORELOCATE"))
            return RPMERR_NORELOCATE;
        if (strEQ(name, "RPMERR_NOSPACE"))
            return RPMERR_NOSPACE;
        if (strEQ(name, "RPMERR_NOSPEC"))
            return RPMERR_NOSPEC;
        if (strEQ(name, "RPMERR_NOTSRPM"))
            return RPMERR_NOTSRPM;
        if (strEQ(name, "RPMERR_NOUSER"))
            return RPMERR_NOUSER;
        if (strEQ(name, "RPMERR_OLDDB"))
            return RPMERR_OLDDB;
        if (strEQ(name, "RPMERR_OLDDBCORRUPT"))
            return RPMERR_OLDDBCORRUPT;
        if (strEQ(name, "RPMERR_OLDDBMISSING"))
            return RPMERR_OLDDBMISSING;
        if (strEQ(name, "RPMERR_OLDPACKAGE"))
            return RPMERR_OLDPACKAGE;
        if (strEQ(name, "RPMERR_PKGINSTALLED"))
            return RPMERR_PKGINSTALLED;
        if (strEQ(name, "RPMERR_READ") || strEQ(name, "RPMERR_READERROR"))
#if (RPM_VERSION >= 0x040002)
            return RPMERR_READ;
#else
            return RPMERR_READERROR;
#endif
        if (strEQ(name, "RPMERR_RENAME"))
            return RPMERR_RENAME;
        if (strEQ(name, "RPMERR_RMDIR"))
            return RPMERR_RMDIR;
        if (strEQ(name, "RPMERR_RPMRC"))
            return RPMERR_RPMRC;
        if (strEQ(name, "RPMERR_SCRIPT"))
            return RPMERR_SCRIPT;
        if (strEQ(name, "RPMERR_SIGGEN"))
            return RPMERR_SIGGEN;
        if (strEQ(name, "RPMERR_STAT"))
            return RPMERR_STAT;
        if (strEQ(name, "RPMERR_UNKNOWNARCH"))
            return RPMERR_UNKNOWNARCH;
        if (strEQ(name, "RPMERR_UNKNOWNOS"))
            return RPMERR_UNKNOWNOS;
        if (strEQ(name, "RPMERR_UNLINK"))
            return RPMERR_UNLINK;
        if (strEQ(name, "RPMERR_UNMATCHEDIF"))
            return RPMERR_UNMATCHEDIF;
        if (strEQ(name, "RPMFILE_CONFIG"))
            return RPMFILE_CONFIG;
        if (strEQ(name, "RPMFILE_DOC"))
            return RPMFILE_DOC;
        if (strEQ(name, "RPMFILE_DONOTUSE"))
            return RPMFILE_DONOTUSE;
        if (strEQ(name, "RPMFILE_GHOST"))
            return RPMFILE_GHOST;
        if (strEQ(name, "RPMFILE_LICENSE"))
            return RPMFILE_LICENSE;
        if (strEQ(name, "RPMFILE_MISSINGOK"))
            return RPMFILE_MISSINGOK;
        if (strEQ(name, "RPMFILE_NOREPLACE"))
            return RPMFILE_NOREPLACE;
        if (strEQ(name, "RPMFILE_README"))
            return RPMFILE_README;
        if (strEQ(name, "RPMFILE_SPECFILE"))
            return RPMFILE_SPECFILE;
        if (strEQ(name, "RPMFILE_STATE_NETSHARED"))
            return RPMFILE_STATE_NETSHARED;
        if (strEQ(name, "RPMFILE_STATE_NORMAL"))
            return RPMFILE_STATE_NORMAL;
        if (strEQ(name, "RPMFILE_STATE_NOTINSTALLED"))
            return RPMFILE_STATE_NOTINSTALLED;
        if (strEQ(name, "RPMFILE_STATE_REPLACED"))
            return RPMFILE_STATE_REPLACED;
        if (strEQ(name, "RPMPROB_FILTER_DISKSPACE"))
            return RPMPROB_FILTER_DISKSPACE;
        if (strEQ(name, "RPMPROB_FILTER_FORCERELOCATE"))
            return RPMPROB_FILTER_FORCERELOCATE;
        if (strEQ(name, "RPMPROB_FILTER_IGNOREARCH"))
            return RPMPROB_FILTER_IGNOREARCH;
        if (strEQ(name, "RPMPROB_FILTER_IGNOREOS"))
            return RPMPROB_FILTER_IGNOREOS;
        if (strEQ(name, "RPMPROB_FILTER_OLDPACKAGE"))
            return RPMPROB_FILTER_OLDPACKAGE;
        if (strEQ(name, "RPMPROB_FILTER_REPLACENEWFILES"))
            return RPMPROB_FILTER_REPLACENEWFILES;
        if (strEQ(name, "RPMPROB_FILTER_REPLACEOLDFILES"))
            return RPMPROB_FILTER_REPLACEOLDFILES;
        if (strEQ(name, "RPMPROB_FILTER_REPLACEPKG"))
            return RPMPROB_FILTER_REPLACEPKG;
        if (strEQ(name, "RPMSENSE_EQUAL"))
            return RPMSENSE_EQUAL;
        if (strEQ(name, "RPMSENSE_GREATER"))
            return RPMSENSE_GREATER;
        if (strEQ(name, "RPMSENSE_LESS"))
            return RPMSENSE_LESS;
        if (strEQ(name, "RPMSENSE_PREREQ"))
            return RPMSENSE_PREREQ;
        if (strEQ(name, "RPMSENSE_SENSEMASK"))
            return RPMSENSE_SENSEMASK;
        if (strEQ(name, "RPMSENSE_TRIGGER"))
            return RPMSENSE_TRIGGER;
        if (strEQ(name, "RPMSENSE_TRIGGERIN"))
            return RPMSENSE_TRIGGERIN;
        if (strEQ(name, "RPMSENSE_TRIGGERPOSTUN"))
            return RPMSENSE_TRIGGERPOSTUN;
        if (strEQ(name, "RPMSENSE_TRIGGERUN"))
            return RPMSENSE_TRIGGERUN;
        if (strEQ(name, "RPMSIGTAG_GPG"))
            return RPMSIGTAG_GPG;
        if (strEQ(name, "RPMSIGTAG_LEMD5_1"))
            return RPMSIGTAG_LEMD5_1;
        if (strEQ(name, "RPMSIGTAG_LEMD5_2"))
            return RPMSIGTAG_LEMD5_2;
        if (strEQ(name, "RPMSIGTAG_MD5"))
            return RPMSIGTAG_MD5;
        if (strEQ(name, "RPMSIGTAG_PGP"))
            return RPMSIGTAG_PGP;
        if (strEQ(name, "RPMSIGTAG_PGP5"))
            return RPMSIGTAG_PGP5;
        if (strEQ(name, "RPMSIGTAG_SIZE"))
            return RPMSIGTAG_SIZE;
        if (strEQ(name, "RPMSIG_BAD"))
            return RPMSIG_BAD;
        if (strEQ(name, "RPMSIG_NOKEY"))
            return RPMSIG_NOKEY;
        if (strEQ(name, "RPMSIG_NOTTRUSTED"))
            return RPMSIG_NOTTRUSTED;
        if (strEQ(name, "RPMSIG_OK"))
            return RPMSIG_OK;
        if (strEQ(name, "RPMSIG_UNKNOWN"))
            return RPMSIG_UNKNOWN;
        if (strEQ(name, "RPMTAG_ARCH"))
            return RPMTAG_ARCH;
        if (strEQ(name, "RPMTAG_ARCHIVESIZE"))
            return RPMTAG_ARCHIVESIZE;
        if (strEQ(name, "RPMTAG_BASENAMES"))
            return RPMTAG_BASENAMES;
        if (strEQ(name, "RPMTAG_BUILDARCHS"))
            return RPMTAG_BUILDARCHS;
        if (strEQ(name, "RPMTAG_BUILDHOST"))
            return RPMTAG_BUILDHOST;
        if (strEQ(name, "RPMTAG_BUILDMACROS"))
            return RPMTAG_BUILDMACROS;
        if (strEQ(name, "RPMTAG_BUILDROOT"))
            return RPMTAG_BUILDROOT;
        if (strEQ(name, "RPMTAG_BUILDTIME"))
            return RPMTAG_BUILDTIME;
        if (strEQ(name, "RPMTAG_CHANGELOGNAME"))
            return RPMTAG_CHANGELOGNAME;
        if (strEQ(name, "RPMTAG_CHANGELOGTEXT"))
            return RPMTAG_CHANGELOGTEXT;
        if (strEQ(name, "RPMTAG_CHANGELOGTIME"))
            return RPMTAG_CHANGELOGTIME;
        if (strEQ(name, "RPMTAG_CONFLICTFLAGS"))
            return RPMTAG_CONFLICTFLAGS;
        if (strEQ(name, "RPMTAG_CONFLICTNAME"))
            return RPMTAG_CONFLICTNAME;
        if (strEQ(name, "RPMTAG_CONFLICTVERSION"))
            return RPMTAG_CONFLICTVERSION;
        if (strEQ(name, "RPMTAG_COPYRIGHT"))
            return RPMTAG_COPYRIGHT;
        if (strEQ(name, "RPMTAG_COOKIE"))
            return RPMTAG_COOKIE;
        if (strEQ(name, "RPMTAG_DESCRIPTION"))
            return RPMTAG_DESCRIPTION;
        if (strEQ(name, "RPMTAG_DIRINDEXES"))
            return RPMTAG_DIRINDEXES;
        if (strEQ(name, "RPMTAG_DIRNAMES"))
            return RPMTAG_DIRNAMES;
        if (strEQ(name, "RPMTAG_DISTRIBUTION"))
            return RPMTAG_DISTRIBUTION;
        if (strEQ(name, "RPMTAG_EXCLUDEARCH"))
            return RPMTAG_EXCLUDEARCH;
        if (strEQ(name, "RPMTAG_EXCLUDEOS"))
            return RPMTAG_EXCLUDEOS;
        if (strEQ(name, "RPMTAG_EXCLUSIVEARCH"))
            return RPMTAG_EXCLUSIVEARCH;
        if (strEQ(name, "RPMTAG_EXCLUSIVEOS"))
            return RPMTAG_EXCLUSIVEOS;
        if (strEQ(name, "RPMTAG_FILEDEVICES"))
            return RPMTAG_FILEDEVICES;
        if (strEQ(name, "RPMTAG_FILEFLAGS"))
            return RPMTAG_FILEFLAGS;
        if (strEQ(name, "RPMTAG_FILEGROUPNAME"))
            return RPMTAG_FILEGROUPNAME;
        if (strEQ(name, "RPMTAG_FILEINODES"))
            return RPMTAG_FILEINODES;
        if (strEQ(name, "RPMTAG_FILELANGS"))
            return RPMTAG_FILELANGS;
        if (strEQ(name, "RPMTAG_FILELINKTOS"))
            return RPMTAG_FILELINKTOS;
        if (strEQ(name, "RPMTAG_FILEMD5S"))
            return RPMTAG_FILEMD5S;
        if (strEQ(name, "RPMTAG_FILEMODES"))
            return RPMTAG_FILEMODES;
        if (strEQ(name, "RPMTAG_FILEMTIMES"))
            return RPMTAG_FILEMTIMES;
        if (strEQ(name, "RPMTAG_FILERDEVS"))
            return RPMTAG_FILERDEVS;
        if (strEQ(name, "RPMTAG_FILESIZES"))
            return RPMTAG_FILESIZES;
        if (strEQ(name, "RPMTAG_FILESTATES"))
            return RPMTAG_FILESTATES;
        if (strEQ(name, "RPMTAG_FILEUSERNAME"))
            return RPMTAG_FILEUSERNAME;
        if (strEQ(name, "RPMTAG_FILEVERIFYFLAGS"))
            return RPMTAG_FILEVERIFYFLAGS;
        if (strEQ(name, "RPMTAG_GIF"))
            return RPMTAG_GIF;
        if (strEQ(name, "RPMTAG_GROUP"))
            return RPMTAG_GROUP;
        if (strEQ(name, "RPMTAG_ICON"))
            return RPMTAG_ICON;
        if (strEQ(name, "RPMTAG_INSTALLTIME"))
            return RPMTAG_INSTALLTIME;
        if (strEQ(name, "RPMTAG_INSTPREFIXES"))
            return RPMTAG_INSTPREFIXES;
        if (strEQ(name, "RPMTAG_LICENSE"))
            return RPMTAG_LICENSE;
        if (strEQ(name, "RPMTAG_NAME"))
            return RPMTAG_NAME;
        if (strEQ(name, "RPMTAG_NOPATCH"))
            return RPMTAG_NOPATCH;
        if (strEQ(name, "RPMTAG_NOSOURCE"))
            return RPMTAG_NOSOURCE;
        if (strEQ(name, "RPMTAG_OBSOLETEFLAGS"))
            return RPMTAG_OBSOLETEFLAGS;
        if (strEQ(name, "RPMTAG_OBSOLETENAME"))
            return RPMTAG_OBSOLETENAME;
        if (strEQ(name, "RPMTAG_OBSOLETEVERSION"))
            return RPMTAG_OBSOLETEVERSION;
        if (strEQ(name, "RPMTAG_OS"))
            return RPMTAG_OS;
        if (strEQ(name, "RPMTAG_PACKAGER"))
            return RPMTAG_PACKAGER;
        if (strEQ(name, "RPMTAG_PATCH"))
            return RPMTAG_PATCH;
        if (strEQ(name, "RPMTAG_POSTIN"))
            return RPMTAG_POSTIN;
        if (strEQ(name, "RPMTAG_POSTINPROG"))
            return RPMTAG_POSTINPROG;
        if (strEQ(name, "RPMTAG_POSTUN"))
            return RPMTAG_POSTUN;
        if (strEQ(name, "RPMTAG_POSTUNPROG"))
            return RPMTAG_POSTUNPROG;
        if (strEQ(name, "RPMTAG_PREFIXES"))
            return RPMTAG_PREFIXES;
        if (strEQ(name, "RPMTAG_PREIN"))
            return RPMTAG_PREIN;
        if (strEQ(name, "RPMTAG_PREINPROG"))
            return RPMTAG_PREINPROG;
        if (strEQ(name, "RPMTAG_PREUN"))
            return RPMTAG_PREUN;
        if (strEQ(name, "RPMTAG_PREUNPROG"))
            return RPMTAG_PREUNPROG;
        if (strEQ(name, "RPMTAG_PROVIDEFLAGS"))
            return RPMTAG_PROVIDEFLAGS;
        if (strEQ(name, "RPMTAG_PROVIDENAME"))
            return RPMTAG_PROVIDENAME;
        if (strEQ(name, "RPMTAG_PROVIDEVERSION"))
            return RPMTAG_PROVIDEVERSION;
        if (strEQ(name, "RPMTAG_RELEASE"))
            return RPMTAG_RELEASE;
        if (strEQ(name, "RPMTAG_REQUIREFLAGS"))
            return RPMTAG_REQUIREFLAGS;
        if (strEQ(name, "RPMTAG_REQUIRENAME"))
            return RPMTAG_REQUIRENAME;
        if (strEQ(name, "RPMTAG_REQUIREVERSION"))
            return RPMTAG_REQUIREVERSION;
        if (strEQ(name, "RPMTAG_RPMVERSION"))
            return RPMTAG_RPMVERSION;
        if (strEQ(name, "RPMTAG_SIZE"))
            return RPMTAG_SIZE;
        if (strEQ(name, "RPMTAG_SOURCE"))
            return RPMTAG_SOURCE;
        if (strEQ(name, "RPMTAG_SOURCERPM"))
            return RPMTAG_SOURCERPM;
        if (strEQ(name, "RPMTAG_SUMMARY"))
            return RPMTAG_SUMMARY;
        if (strEQ(name, "RPMTAG_TRIGGERFLAGS"))
            return RPMTAG_TRIGGERFLAGS;
        if (strEQ(name, "RPMTAG_TRIGGERINDEX"))
            return RPMTAG_TRIGGERINDEX;
        if (strEQ(name, "RPMTAG_TRIGGERNAME"))
            return RPMTAG_TRIGGERNAME;
        if (strEQ(name, "RPMTAG_TRIGGERSCRIPTPROG"))
            return RPMTAG_TRIGGERSCRIPTPROG;
        if (strEQ(name, "RPMTAG_TRIGGERSCRIPTS"))
            return RPMTAG_TRIGGERSCRIPTS;
        if (strEQ(name, "RPMTAG_TRIGGERVERSION"))
            return RPMTAG_TRIGGERVERSION;
        if (strEQ(name, "RPMTAG_URL"))
            return RPMTAG_URL;
        if (strEQ(name, "RPMTAG_VENDOR"))
            return RPMTAG_VENDOR;
        if (strEQ(name, "RPMTAG_VERIFYSCRIPT"))
            return RPMTAG_VERIFYSCRIPT;
        if (strEQ(name, "RPMTAG_VERIFYSCRIPTPROG"))
            return RPMTAG_VERIFYSCRIPTPROG;
        if (strEQ(name, "RPMTAG_VERSION"))
            return RPMTAG_VERSION;
        if (strEQ(name, "RPMTAG_XPM"))
            return RPMTAG_XPM;
        if (strEQ(name, "RPMTRANS_FLAG_ALLFILES"))
            return RPMTRANS_FLAG_ALLFILES;
        if (strEQ(name, "RPMTRANS_FLAG_BUILD_PROBS"))
            return RPMTRANS_FLAG_BUILD_PROBS;
        if (strEQ(name, "RPMTRANS_FLAG_JUSTDB"))
            return RPMTRANS_FLAG_JUSTDB;
        if (strEQ(name, "RPMTRANS_FLAG_KEEPOBSOLETE"))
            return RPMTRANS_FLAG_KEEPOBSOLETE;
        if (strEQ(name, "RPMTRANS_FLAG_NODOCS"))
            return RPMTRANS_FLAG_NODOCS;
        if (strEQ(name, "RPMTRANS_FLAG_NOSCRIPTS"))
            return RPMTRANS_FLAG_NOSCRIPTS;
        if (strEQ(name, "RPMTRANS_FLAG_NOTRIGGERS"))
            return RPMTRANS_FLAG_NOTRIGGERS;
        if (strEQ(name, "RPMTRANS_FLAG_TEST"))
            return RPMTRANS_FLAG_TEST;
        if (strEQ(name, "RPMVAR_INCLUDE"))
            return RPMVAR_INCLUDE;
        if (strEQ(name, "RPMVAR_MACROFILES"))
            return RPMVAR_MACROFILES;
        if (strEQ(name, "RPMVAR_NUM"))
            return RPMVAR_NUM;
        if (strEQ(name, "RPMVAR_OPTFLAGS"))
            return RPMVAR_OPTFLAGS;
        if (strEQ(name, "RPMVAR_PROVIDES"))
            return RPMVAR_PROVIDES;
        if (strEQ(name, "RPMVERIFY_ALL"))
            return RPMVERIFY_ALL;
        if (strEQ(name, "RPMVERIFY_FILESIZE"))
            return RPMVERIFY_FILESIZE;
        if (strEQ(name, "RPMVERIFY_GROUP"))
            return RPMVERIFY_GROUP;
        if (strEQ(name, "RPMVERIFY_LINKTO"))
            return RPMVERIFY_LINKTO;
        if (strEQ(name, "RPMVERIFY_LSTATFAIL"))
            return RPMVERIFY_LSTATFAIL;
        if (strEQ(name, "RPMVERIFY_MD5"))
            return RPMVERIFY_MD5;
        if (strEQ(name, "RPMVERIFY_MODE"))
            return RPMVERIFY_MODE;
        if (strEQ(name, "RPMVERIFY_MTIME"))
            return RPMVERIFY_MTIME;
        if (strEQ(name, "RPMVERIFY_NONE"))
            return RPMVERIFY_NONE;
        if (strEQ(name, "RPMVERIFY_RDEV"))
            return RPMVERIFY_RDEV;
        if (strEQ(name, "RPMVERIFY_READFAIL"))
            return RPMVERIFY_READFAIL;
        if (strEQ(name, "RPMVERIFY_READLINKFAIL"))
            return RPMVERIFY_READLINKFAIL;
        if (strEQ(name, "RPMVERIFY_USER"))
            return RPMVERIFY_USER;
        if (strEQ(name, "RPM_MACHTABLE_BUILDARCH"))
            return RPM_MACHTABLE_BUILDARCH;
        if (strEQ(name, "RPM_MACHTABLE_BUILDOS"))
            return RPM_MACHTABLE_BUILDOS;
        if (strEQ(name, "RPM_MACHTABLE_COUNT"))
            return RPM_MACHTABLE_COUNT;
        if (strEQ(name, "RPM_MACHTABLE_INSTARCH"))
            return RPM_MACHTABLE_INSTARCH;
        if (strEQ(name, "RPM_MACHTABLE_INSTOS"))
            return RPM_MACHTABLE_INSTOS;
        break;
    case 'S':
        break;
    case 'T':
        break;
    case 'U':
        if (strEQ(name, "UNINSTALL_ALLMATCHES"))
            return UNINSTALL_ALLMATCHES;
        if (strEQ(name, "UNINSTALL_NODEPS"))
            return UNINSTALL_NODEPS;
        break;
    case 'V':
        if (strEQ(name, "VERIFY_DEPS"))
            return VERIFY_DEPS;
        if (strEQ(name, "VERIFY_FILES"))
            return VERIFY_FILES;
        if (strEQ(name, "VERIFY_MD5"))
            return VERIFY_MD5;
        if (strEQ(name, "VERIFY_SCRIPT"))
            return VERIFY_SCRIPT;
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
}


MODULE = RPM::Constants PACKAGE = RPM::Constants


int
constant(name)
    char* name;
    PROTOTYPE: $
    CODE:
    RETVAL = constant(aTHX_ name);
    OUTPUT:
    RETVAL
