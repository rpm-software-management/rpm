#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Constants.xs,v 1.11 2001/05/15 07:49:56 rjray Exp $";

static int constant(pTHX_ char *name)
{
    errno = 0;

    switch (*name)
    {
      case 'C':
        if (strnEQ(name, "CHECKSIG_", 9))
        {
            if (strEQ(name + 9, "GPG"))
                return CHECKSIG_GPG;
            if (strEQ(name + 9, "MD5"))
                return CHECKSIG_MD5;
            if (strEQ(name + 9, "PGP"))
                return CHECKSIG_PGP;
        }
        break;
      case 'I':
        if (strnEQ(name, "INSTALL_", 8))
        {
            if (strEQ(name + 8, "HASH"))
                return INSTALL_HASH;
            if (strEQ(name + 8, "LABEL"))
                return INSTALL_LABEL;
            if (strEQ(name + 8, "NODEPS"))
                return INSTALL_NODEPS;
            if (strEQ(name + 8, "NOORDER"))
                return INSTALL_NOORDER;
            if (strEQ(name + 8, "PERCENT"))
                return INSTALL_PERCENT;
            if (strEQ(name + 8, "UPGRADE"))
                return INSTALL_UPGRADE;
        }
        break;
      case 'Q':
        if (strnEQ(name, "QUERY_FOR_", 10))
        {
            if (strEQ(name + 10, "CONFIG"))
                return QUERY_FOR_CONFIG;
            if (strEQ(name + 10, "DOCS"))
                return QUERY_FOR_DOCS;
            if (strEQ(name + 10, "DUMPFILES"))
                return QUERY_FOR_DUMPFILES;
            if (strEQ(name + 10, "LIST"))
                return QUERY_FOR_LIST;
            if (strEQ(name + 10, "STATE"))
                return QUERY_FOR_STATE;
        }
        break;
      case 'R':
        /* THIS is the area that really needs progressive breaking down by
           minimal leading-string matching. */
        if (strnEQ(name, "RPM_MACHTABLE_", 14))
        {
            if (strEQ(name + 14, "BUILDARCH"))
                return RPM_MACHTABLE_BUILDARCH;
            if (strEQ(name + 14, "BUILDOS"))
                return RPM_MACHTABLE_BUILDOS;
            if (strEQ(name + 14, "COUNT"))
                return RPM_MACHTABLE_COUNT;
            if (strEQ(name + 14, "INSTARCH"))
                return RPM_MACHTABLE_INSTARCH;
            if (strEQ(name + 14, "INSTOS"))
                return RPM_MACHTABLE_INSTOS;
        }
        if (strnEQ(name, "RPM_", 4))
        {
            if (strEQ(name + 4, "NULL_TYPE"))
                return RPM_NULL_TYPE;
            if (strEQ(name + 4, "CHAR_TYPE"))
                return RPM_CHAR_TYPE;
            if (strEQ(name + 4, "INT8_TYPE"))
                return RPM_INT8_TYPE;
            if (strEQ(name + 4, "INT16_TYPE"))
                return RPM_INT16_TYPE;
            if (strEQ(name + 4, "INT32_TYPE"))
                return RPM_INT32_TYPE;
            if (strEQ(name + 4, "STRING_TYPE"))
                return RPM_STRING_TYPE;
            if (strEQ(name + 4, "BIN_TYPE"))
                return RPM_BIN_TYPE;
            if (strEQ(name + 4, "STRING_ARRAY_TYPE"))
                return RPM_STRING_ARRAY_TYPE;
            if (strEQ(name + 4, "I18NSTRING_TYPE"))
                return RPM_I18NSTRING_TYPE;
        }
        if (strnEQ(name, "RPMERR_", 7))
        {
            switch (*(name + 7))
            {
              case 'B':
                if (strEQ(name + 7, "BADARG"))
                    return RPMERR_BADARG;
                if (strEQ(name + 7, "BADDEV"))
                    return RPMERR_BADDEV;
                if (strEQ(name + 7, "BADFILENAME"))
                    return RPMERR_BADFILENAME;
                if (strEQ(name + 7, "BADMAGIC"))
                    return RPMERR_BADMAGIC;
                if (strEQ(name + 7, "BADRELOCATE"))
                    return RPMERR_BADRELOCATE;
                if (strEQ(name + 7, "BADSIGTYPE"))
                    return RPMERR_BADSIGTYPE;
                if (strEQ(name + 7, "BADSPEC"))
                    return RPMERR_BADSPEC;
                break;
              case 'C':
                if (strEQ(name + 7, "CHOWN"))
                    return RPMERR_CHOWN;
                if (strEQ(name + 7, "CPIO"))
                    return RPMERR_CPIO;
                if (strEQ(name + 7, "CREATE"))
                    return RPMERR_CREATE;
                break;
              case 'D':
                if (strEQ(name + 7, "DBCORRUPT"))
                    return RPMERR_DBCORRUPT;
                if (strEQ(name + 7, "DBGETINDEX"))
                    return RPMERR_DBGETINDEX;
                if (strEQ(name + 7, "DBOPEN"))
                    return RPMERR_DBOPEN;
                if (strEQ(name + 7, "DBPUTINDEX"))
                    return RPMERR_DBPUTINDEX;
                break;
              case 'E':
                if (strEQ(name + 7, "EXEC"))
                    return RPMERR_EXEC;
                break;
              case 'F':
                if (strEQ(name + 7, "FILECONFLICT"))
                    return RPMERR_FILECONFLICT;
                if (strEQ(name + 7, "FLOCK"))
                    return RPMERR_FLOCK;
                if (strEQ(name + 7, "FORK"))
                    return RPMERR_FORK;
                break;
              case 'G':
                if (strEQ(name + 7, "GDBMOPEN"))
                    return RPMERR_GDBMOPEN;
                if (strEQ(name + 7, "GDBMREAD"))
                    return RPMERR_GDBMREAD;
                if (strEQ(name + 7, "GDBMWRITE"))
                    return RPMERR_GDBMWRITE;
                if (strEQ(name + 7, "GZIP"))
                    return RPMERR_GZIP;
                break;
              case 'I':
                if (strEQ(name + 7, "INTERNAL"))
                    return RPMERR_INTERNAL;
                break;
              case 'L':
                if (strEQ(name + 7, "LDD"))
                    return RPMERR_LDD;
                break;
              case 'M':
                if (strEQ(name + 7, "MKDIR"))
                    return RPMERR_MKDIR;
                if (strEQ(name + 7, "MTAB"))
                    return RPMERR_MTAB;
                break;
              case 'N':
                if (strEQ(name + 7, "NEWPACKAGE"))
                    return RPMERR_NEWPACKAGE;
                if (strEQ(name + 7, "NOCREATEDB"))
                    return RPMERR_NOCREATEDB;
                if (strEQ(name + 7, "NOGROUP"))
                    return RPMERR_NOGROUP;
                if (strEQ(name + 7, "NORELOCATE"))
                    return RPMERR_NORELOCATE;
                if (strEQ(name + 7, "NOSPACE"))
                    return RPMERR_NOSPACE;
                if (strEQ(name + 7, "NOSPEC"))
                    return RPMERR_NOSPEC;
                if (strEQ(name + 7, "NOTSRPM"))
                    return RPMERR_NOTSRPM;
                if (strEQ(name + 7, "NOUSER"))
                    return RPMERR_NOUSER;
                break;
              case 'O':
                if (strEQ(name + 7, "OLDDB"))
                    return RPMERR_OLDDB;
                if (strEQ(name + 7, "OLDDBCORRUPT"))
                    return RPMERR_OLDDBCORRUPT;
                if (strEQ(name + 7, "OLDDBMISSING"))
                    return RPMERR_OLDDBMISSING;
                if (strEQ(name + 7, "OLDPACKAGE"))
                    return RPMERR_OLDPACKAGE;
                break;
              case 'P':
                if (strEQ(name + 7, "PKGINSTALLED"))
                    return RPMERR_PKGINSTALLED;
                break;
              case 'R':
                if (strEQ(name + 7, "READ") || strEQ(name + 7, "READERROR"))
#                 if (RPM_VERSION >= 0x040002)
                    return RPMERR_READ;
#                 else
                    return RPMERR_READERROR;
#                 endif
                if (strEQ(name + 7, "RENAME"))
                    return RPMERR_RENAME;
                if (strEQ(name + 7, "RMDIR"))
                    return RPMERR_RMDIR;
                if (strEQ(name + 7, "RPMRC"))
                    return RPMERR_RPMRC;
                break;
              case 'S':
                if (strEQ(name + 7, "SCRIPT"))
                    return RPMERR_SCRIPT;
                if (strEQ(name + 7, "SIGGEN"))
                    return RPMERR_SIGGEN;
                if (strEQ(name + 7, "STAT"))
                    return RPMERR_STAT;
                break;
              case 'U':
                if (strEQ(name + 7, "UNKNOWNARCH"))
                    return RPMERR_UNKNOWNARCH;
                if (strEQ(name + 7, "UNKNOWNOS"))
                    return RPMERR_UNKNOWNOS;
                if (strEQ(name + 7, "UNLINK"))
                    return RPMERR_UNLINK;
                if (strEQ(name + 7, "UNMATCHEDIF"))
                    return RPMERR_UNMATCHEDIF;
                break;
              default:
                break;
            }
        }
        if (strnEQ(name, "RPMFILE_", 8))
        {
            if (strEQ(name + 8, "CONFIG"))
                return RPMFILE_CONFIG;
            if (strEQ(name + 8, "DOC"))
                return RPMFILE_DOC;
            if (strEQ(name + 8, "DONOTUSE"))
                return RPMFILE_DONOTUSE;
            if (strEQ(name + 8, "GHOST"))
                return RPMFILE_GHOST;
            if (strEQ(name + 8, "LICENSE"))
                return RPMFILE_LICENSE;
            if (strEQ(name + 8, "MISSINGOK"))
                return RPMFILE_MISSINGOK;
            if (strEQ(name + 8, "NOREPLACE"))
                return RPMFILE_NOREPLACE;
            if (strEQ(name + 8, "README"))
                return RPMFILE_README;
            if (strEQ(name + 8, "SPECFILE"))
                return RPMFILE_SPECFILE;
            if (strEQ(name + 8, "STATE_NETSHARED"))
                return RPMFILE_STATE_NETSHARED;
            if (strEQ(name + 8, "STATE_NORMAL"))
                return RPMFILE_STATE_NORMAL;
            if (strEQ(name + 8, "STATE_NOTINSTALLED"))
                return RPMFILE_STATE_NOTINSTALLED;
            if (strEQ(name + 8, "STATE_REPLACED"))
                return RPMFILE_STATE_REPLACED;
        }
        if (strnEQ(name, "RPMPROB_FILTER_", 15))
        {
            if (strEQ(name + 15, "DISKSPACE"))
                return RPMPROB_FILTER_DISKSPACE;
            if (strEQ(name + 15, "FORCERELOCATE"))
                return RPMPROB_FILTER_FORCERELOCATE;
            if (strEQ(name + 15, "IGNOREARCH"))
                return RPMPROB_FILTER_IGNOREARCH;
            if (strEQ(name + 15, "IGNOREOS"))
                return RPMPROB_FILTER_IGNOREOS;
            if (strEQ(name + 15, "OLDPACKAGE"))
                return RPMPROB_FILTER_OLDPACKAGE;
            if (strEQ(name + 15, "REPLACENEWFILES"))
                return RPMPROB_FILTER_REPLACENEWFILES;
            if (strEQ(name + 15, "REPLACEOLDFILES"))
                return RPMPROB_FILTER_REPLACEOLDFILES;
            if (strEQ(name + 15, "REPLACEPKG"))
                return RPMPROB_FILTER_REPLACEPKG;
        }
        if (strnEQ(name, "RPMSENSE_", 9))
        {
            if (strEQ(name + 9, "EQUAL"))
                return RPMSENSE_EQUAL;
            if (strEQ(name + 9, "GREATER"))
                return RPMSENSE_GREATER;
            if (strEQ(name + 9, "LESS"))
                return RPMSENSE_LESS;
            if (strEQ(name + 9, "PREREQ"))
                return RPMSENSE_PREREQ;
            if (strEQ(name + 9, "SENSEMASK"))
                return RPMSENSE_SENSEMASK;
            if (strEQ(name + 9, "TRIGGER"))
                return RPMSENSE_TRIGGER;
            if (strEQ(name + 9, "TRIGGERIN"))
                return RPMSENSE_TRIGGERIN;
            if (strEQ(name + 9, "TRIGGERPOSTUN"))
                return RPMSENSE_TRIGGERPOSTUN;
            if (strEQ(name + 9, "TRIGGERUN"))
                return RPMSENSE_TRIGGERUN;
        }
        if (strnEQ(name, "RPMSIGTAG_", 10))
        {
            if (strEQ(name + 10, "GPG"))
                return RPMSIGTAG_GPG;
            if (strEQ(name + 10, "LEMD5_1"))
                return RPMSIGTAG_LEMD5_1;
            if (strEQ(name + 10, "LEMD5_2"))
                return RPMSIGTAG_LEMD5_2;
            if (strEQ(name + 10, "MD5"))
                return RPMSIGTAG_MD5;
            if (strEQ(name + 10, "PGP"))
                return RPMSIGTAG_PGP;
            if (strEQ(name + 10, "PGP5"))
                return RPMSIGTAG_PGP5;
            if (strEQ(name + 10, "SIZE"))
                return RPMSIGTAG_SIZE;
        }
        if (strnEQ(name, "RPMSIG_", 7))
        {
            if (strEQ(name + 7, "BAD"))
                return RPMSIG_BAD;
            if (strEQ(name + 7, "NOKEY"))
                return RPMSIG_NOKEY;
            if (strEQ(name + 7, "NOTTRUSTED"))
                return RPMSIG_NOTTRUSTED;
            if (strEQ(name + 7, "OK"))
                return RPMSIG_OK;
            if (strEQ(name + 7, "UNKNOWN"))
                return RPMSIG_UNKNOWN;
        }
        if (strnEQ(name, "RPMTAG_", 7))
        {
            switch (*(name + 7))
            {
              case 'A':
                if (strEQ(name + 7, "ARCH"))
                    return RPMTAG_ARCH;
                if (strEQ(name + 7, "ARCHIVESIZE"))
                    return RPMTAG_ARCHIVESIZE;
                break;
              case 'B':
                if (strEQ(name + 7, "BASENAMES"))
                    return RPMTAG_BASENAMES;
                if (strEQ(name + 7, "BUILDARCHS"))
                    return RPMTAG_BUILDARCHS;
                if (strEQ(name + 7, "BUILDHOST"))
                    return RPMTAG_BUILDHOST;
                if (strEQ(name + 7, "BUILDMACROS"))
                    return RPMTAG_BUILDMACROS;
                if (strEQ(name + 7, "BUILDROOT"))
                    return RPMTAG_BUILDROOT;
                if (strEQ(name + 7, "BUILDTIME"))
                    return RPMTAG_BUILDTIME;
                break;
              case 'C':
                if (strEQ(name + 7, "CHANGELOGNAME"))
                    return RPMTAG_CHANGELOGNAME;
                if (strEQ(name + 7, "CHANGELOGTEXT"))
                    return RPMTAG_CHANGELOGTEXT;
                if (strEQ(name + 7, "CHANGELOGTIME"))
                    return RPMTAG_CHANGELOGTIME;
                if (strEQ(name + 7, "CONFLICTFLAGS"))
                    return RPMTAG_CONFLICTFLAGS;
                if (strEQ(name + 7, "CONFLICTNAME"))
                    return RPMTAG_CONFLICTNAME;
                if (strEQ(name + 7, "CONFLICTVERSION"))
                    return RPMTAG_CONFLICTVERSION;
                if (strEQ(name + 7, "COPYRIGHT"))
                    return RPMTAG_COPYRIGHT;
                if (strEQ(name + 7, "COOKIE"))
                    return RPMTAG_COOKIE;
                break;
              case 'D':
                if (strEQ(name + 7, "DESCRIPTION"))
                    return RPMTAG_DESCRIPTION;
                if (strEQ(name + 7, "DIRINDEXES"))
                    return RPMTAG_DIRINDEXES;
                if (strEQ(name + 7, "DIRNAMES"))
                    return RPMTAG_DIRNAMES;
                if (strEQ(name + 7, "DISTRIBUTION"))
                    return RPMTAG_DISTRIBUTION;
                break;
              case 'E':
                if (strEQ(name + 7, "EXCLUDEARCH"))
                    return RPMTAG_EXCLUDEARCH;
                if (strEQ(name + 7, "EXCLUDEOS"))
                    return RPMTAG_EXCLUDEOS;
                if (strEQ(name + 7, "EXCLUSIVEARCH"))
                    return RPMTAG_EXCLUSIVEARCH;
                if (strEQ(name + 7, "EXCLUSIVEOS"))
                    return RPMTAG_EXCLUSIVEOS;
                break;
              case 'F':
                if (strEQ(name + 7, "FILEDEVICES"))
                    return RPMTAG_FILEDEVICES;
                if (strEQ(name + 7, "FILEFLAGS"))
                    return RPMTAG_FILEFLAGS;
                if (strEQ(name + 7, "FILEGROUPNAME"))
                    return RPMTAG_FILEGROUPNAME;
                if (strEQ(name + 7, "FILEINODES"))
                    return RPMTAG_FILEINODES;
                if (strEQ(name + 7, "FILELANGS"))
                    return RPMTAG_FILELANGS;
                if (strEQ(name + 7, "FILELINKTOS"))
                    return RPMTAG_FILELINKTOS;
                if (strEQ(name + 7, "FILEMD5S"))
                    return RPMTAG_FILEMD5S;
                if (strEQ(name + 7, "FILEMODES"))
                    return RPMTAG_FILEMODES;
                if (strEQ(name + 7, "FILEMTIMES"))
                    return RPMTAG_FILEMTIMES;
                if (strEQ(name + 7, "FILERDEVS"))
                    return RPMTAG_FILERDEVS;
                if (strEQ(name + 7, "FILESIZES"))
                    return RPMTAG_FILESIZES;
                if (strEQ(name + 7, "FILESTATES"))
                    return RPMTAG_FILESTATES;
                if (strEQ(name + 7, "FILEUSERNAME"))
                    return RPMTAG_FILEUSERNAME;
                if (strEQ(name + 7, "FILEVERIFYFLAGS"))
                    return RPMTAG_FILEVERIFYFLAGS;
                break;
              case 'G':
                if (strEQ(name + 7, "GIF"))
                    return RPMTAG_GIF;
                if (strEQ(name + 7, "GROUP"))
                    return RPMTAG_GROUP;
                break;
              case 'I':
                if (strEQ(name + 7, "ICON"))
                    return RPMTAG_ICON;
                if (strEQ(name + 7, "INSTALLTIME"))
                    return RPMTAG_INSTALLTIME;
                if (strEQ(name + 7, "INSTPREFIXES"))
                    return RPMTAG_INSTPREFIXES;
                break;
              case 'L':
                if (strEQ(name + 7, "LICENSE"))
                    return RPMTAG_LICENSE;
                break;
              case 'N':
                if (strEQ(name + 7, "NAME"))
                    return RPMTAG_NAME;
                if (strEQ(name + 7, "NOPATCH"))
                    return RPMTAG_NOPATCH;
                if (strEQ(name + 7, "NOSOURCE"))
                    return RPMTAG_NOSOURCE;
                break;
              case 'O':
                if (strEQ(name + 7, "OBSOLETEFLAGS"))
                    return RPMTAG_OBSOLETEFLAGS;
                if (strEQ(name + 7, "OBSOLETENAME"))
                    return RPMTAG_OBSOLETENAME;
                if (strEQ(name + 7, "OBSOLETEVERSION"))
                    return RPMTAG_OBSOLETEVERSION;
                if (strEQ(name + 7, "OS"))
                    return RPMTAG_OS;
                break;
              case 'P':
                if (strEQ(name + 7, "PACKAGER"))
                    return RPMTAG_PACKAGER;
                if (strEQ(name + 7, "PATCH"))
                    return RPMTAG_PATCH;
                if (strEQ(name + 7, "POSTIN"))
                    return RPMTAG_POSTIN;
                if (strEQ(name + 7, "POSTINPROG"))
                    return RPMTAG_POSTINPROG;
                if (strEQ(name + 7, "POSTUN"))
                    return RPMTAG_POSTUN;
                if (strEQ(name + 7, "POSTUNPROG"))
                    return RPMTAG_POSTUNPROG;
                if (strEQ(name + 7, "PREFIXES"))
                    return RPMTAG_PREFIXES;
                if (strEQ(name + 7, "PREIN"))
                    return RPMTAG_PREIN;
                if (strEQ(name + 7, "PREINPROG"))
                    return RPMTAG_PREINPROG;
                if (strEQ(name + 7, "PREUN"))
                    return RPMTAG_PREUN;
                if (strEQ(name + 7, "PREUNPROG"))
                    return RPMTAG_PREUNPROG;
                if (strEQ(name + 7, "PROVIDEFLAGS"))
                    return RPMTAG_PROVIDEFLAGS;
                if (strEQ(name + 7, "PROVIDENAME"))
                    return RPMTAG_PROVIDENAME;
                if (strEQ(name + 7, "PROVIDEVERSION"))
                    return RPMTAG_PROVIDEVERSION;
                break;
              case 'R':
                if (strEQ(name + 7, "RELEASE"))
                    return RPMTAG_RELEASE;
                if (strEQ(name + 7, "REQUIREFLAGS"))
                    return RPMTAG_REQUIREFLAGS;
                if (strEQ(name + 7, "REQUIRENAME"))
                    return RPMTAG_REQUIRENAME;
                if (strEQ(name + 7, "REQUIREVERSION"))
                    return RPMTAG_REQUIREVERSION;
                if (strEQ(name + 7, "RPMVERSION"))
                    return RPMTAG_RPMVERSION;
                break;
              case 'S':
                if (strEQ(name + 7, "SIZE"))
                    return RPMTAG_SIZE;
                if (strEQ(name + 7, "SOURCE"))
                    return RPMTAG_SOURCE;
                if (strEQ(name + 7, "SOURCERPM"))
                    return RPMTAG_SOURCERPM;
                if (strEQ(name + 7, "SUMMARY"))
                    return RPMTAG_SUMMARY;
                break;
              case 'T':
                if (strEQ(name + 7, "TRIGGERFLAGS"))
                    return RPMTAG_TRIGGERFLAGS;
                if (strEQ(name + 7, "TRIGGERINDEX"))
                    return RPMTAG_TRIGGERINDEX;
                if (strEQ(name + 7, "TRIGGERNAME"))
                    return RPMTAG_TRIGGERNAME;
                if (strEQ(name + 7, "TRIGGERSCRIPTPROG"))
                    return RPMTAG_TRIGGERSCRIPTPROG;
                if (strEQ(name + 7, "TRIGGERSCRIPTS"))
                    return RPMTAG_TRIGGERSCRIPTS;
                if (strEQ(name + 7, "TRIGGERVERSION"))
                    return RPMTAG_TRIGGERVERSION;
                break;
              case 'U':
                if (strEQ(name + 7, "URL"))
                    return RPMTAG_URL;
                break;
              case 'V':
                if (strEQ(name + 7, "VENDOR"))
                    return RPMTAG_VENDOR;
                if (strEQ(name + 7, "VERIFYSCRIPT"))
                    return RPMTAG_VERIFYSCRIPT;
                if (strEQ(name + 7, "VERIFYSCRIPTPROG"))
                    return RPMTAG_VERIFYSCRIPTPROG;
                if (strEQ(name + 7, "VERSION"))
                    return RPMTAG_VERSION;
                break;
              case 'X':
                if (strEQ(name + 7, "XPM"))
                    return RPMTAG_XPM;
                break;
              default:
                break;
            }
        }
        if (strnEQ(name, "RPMTRANS_FLAG_", 14))
        {
            if (strEQ(name + 14, "ALLFILES"))
                return RPMTRANS_FLAG_ALLFILES;
            if (strEQ(name + 14, "BUILD_PROBS"))
                return RPMTRANS_FLAG_BUILD_PROBS;
            if (strEQ(name + 14, "JUSTDB"))
                return RPMTRANS_FLAG_JUSTDB;
            if (strEQ(name + 14, "KEEPOBSOLETE"))
                return RPMTRANS_FLAG_KEEPOBSOLETE;
            if (strEQ(name + 14, "NODOCS"))
                return RPMTRANS_FLAG_NODOCS;
            if (strEQ(name + 14, "NOSCRIPTS"))
                return RPMTRANS_FLAG_NOSCRIPTS;
            if (strEQ(name + 14, "NOTRIGGERS"))
                return RPMTRANS_FLAG_NOTRIGGERS;
            if (strEQ(name + 14, "TEST"))
                return RPMTRANS_FLAG_TEST;
        }
        if (strnEQ(name, "RPMVAR_", 7))
        {
            if (strEQ(name + 7, "INCLUDE"))
                return RPMVAR_INCLUDE;
            if (strEQ(name + 7, "MACROFILES"))
                return RPMVAR_MACROFILES;
            if (strEQ(name + 7, "NUM"))
                return RPMVAR_NUM;
            if (strEQ(name + 7, "OPTFLAGS"))
                return RPMVAR_OPTFLAGS;
            if (strEQ(name + 7, "PROVIDES"))
                return RPMVAR_PROVIDES;
        }
        if (strnEQ(name, "RPMVERIFY_", 10))
        {
            if (strEQ(name + 10, "ALL"))
                return RPMVERIFY_ALL;
            if (strEQ(name + 10, "FILESIZE"))
                return RPMVERIFY_FILESIZE;
            if (strEQ(name + 10, "GROUP"))
                return RPMVERIFY_GROUP;
            if (strEQ(name + 10, "LINKTO"))
                return RPMVERIFY_LINKTO;
            if (strEQ(name + 10, "LSTATFAIL"))
                return RPMVERIFY_LSTATFAIL;
            if (strEQ(name + 10, "MD5"))
                return RPMVERIFY_MD5;
            if (strEQ(name + 10, "MODE"))
                return RPMVERIFY_MODE;
            if (strEQ(name + 10, "MTIME"))
                return RPMVERIFY_MTIME;
            if (strEQ(name + 10, "NONE"))
                return RPMVERIFY_NONE;
            if (strEQ(name + 10, "RDEV"))
                return RPMVERIFY_RDEV;
            if (strEQ(name + 10, "READFAIL"))
                return RPMVERIFY_READFAIL;
            if (strEQ(name + 10, "READLINKFAIL"))
                return RPMVERIFY_READLINKFAIL;
            if (strEQ(name + 10, "USER"))
                return RPMVERIFY_USER;
        }
        break;
      case 'U':
        if (strEQ(name, "UNINSTALL_ALLMATCHES"))
            return UNINSTALL_ALLMATCHES;
        if (strEQ(name, "UNINSTALL_NODEPS"))
            return UNINSTALL_NODEPS;
        break;
      case 'V':
        if (strnEQ(name, "VERIFY_", 7))
        {
            if (strEQ(name + 7, "DEPS"))
                return VERIFY_DEPS;
            if (strEQ(name + 7, "FILES"))
                return VERIFY_FILES;
            if (strEQ(name + 7, "MD5"))
                return VERIFY_MD5;
            if (strEQ(name + 7, "SCRIPT"))
                return VERIFY_SCRIPT;
        }
        break;
      default:
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
