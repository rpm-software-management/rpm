#ifndef H_RPMERR
#define H_RPMERR

/** \ingroup rpmio
 * \file rpmio/rpmerr.h
 */

/**
 * Tokens used by rpmError().
 */
typedef enum rpmerrCode_e {
    RPMERR_GDBMOPEN	= -2,	/*!< gdbm open failed */
    RPMERR_GDBMREAD	= -3,	/*!< gdbm read failed */
    RPMERR_GDBMWRITE	= -4,	/*!< gdbm write failed */
    RPMERR_INTERNAL	= -5,	/*!< internal RPM error */
    RPMERR_DBCORRUPT	= -6,	/*!< rpm database is corrupt */
    RPMERR_OLDDBCORRUPT	= -7,	/*!< old style rpm database is corrupt */
    RPMERR_OLDDBMISSING	= -8,	/*!< old style rpm database is missing */
    RPMERR_NOCREATEDB	= -9,	/*!< cannot create new database */
    RPMERR_DBOPEN	= -10,	/*!< database open failed */
    RPMERR_DBGETINDEX	= -11,	/*!< database get from index failed */
    RPMERR_DBPUTINDEX	= -12,	/*!< database get from index failed */
    RPMERR_NEWPACKAGE	= -13,	/*!< package is too new to handle */
    RPMERR_BADMAGIC	= -14,	/*!< bad magic for an RPM */
    RPMERR_RENAME	= -15,	/*!< rename(2) failed */
    RPMERR_UNLINK	= -16,	/*!< unlink(2) failed */
    RPMERR_RMDIR	= -17,	/*!< rmdir(2) failed */
    RPMERR_PKGINSTALLED	= -18,	/*!< package already installed */
    RPMERR_CHOWN	= -19,	/*!< chown() call failed */
    RPMERR_NOUSER	= -20,	/*!< user does not exist */
    RPMERR_NOGROUP	= -21,	/*!< group does not exist */
    RPMERR_MKDIR	= -22,	/*!< mkdir() call failed */
    RPMERR_FILECONFLICT	= -23,	/*!< file being installed exists */
    RPMERR_RPMRC	= -24,	/*!< bad line in rpmrc */
    RPMERR_NOSPEC	= -25,	/*!< .spec file is missing */
    RPMERR_NOTSRPM	= -26,	/*!< a source rpm was expected */
    RPMERR_FLOCK	= -27,	/*!< locking the database failed */
    RPMERR_OLDPACKAGE	= -28,	/*!< trying upgrading to old version */
/*    	RPMERR_BADARCH  = -29,	bad architecture or arch mismatch */
    RPMERR_CREATE	= -30,	/*!< failed to create a file */
    RPMERR_NOSPACE	= -31,	/*!< out of disk space */
    RPMERR_NORELOCATE	= -32,	/*!< tried to do improper relocatation */
/*    	RPMERR_BADOS    = -33,	bad architecture or arch mismatch */
    RPMMESS_BACKUP	= -34,	/*!< backup made during [un]install */
    RPMERR_MTAB		= -35,	/*!< failed to read mount table */
    RPMERR_STAT		= -36,	/*!< failed to stat something */
    RPMERR_BADDEV	= -37,	/*!< file on device not listed in mtab */
    RPMMESS_ALTNAME	= -38,	/*!< file written as .rpmnew */
    RPMMESS_PREREQLOOP	= -39,	/*!< loop in prerequisites */
    RPMERR_BADRELOCATE	= -40,	/*!< bad relocation was specified */
    RPMERR_OLDDB	= -41,	/*!< old format database */

/* spec.c build.c pack.c */
    RPMERR_UNMATCHEDIF	= -107,	/*!< unclosed %ifarch or %ifos */
    RPMERR_BADARG	= -109,	/*!< @todo Document. */
    RPMERR_SCRIPT	= -110,	/*!< errors related to script exec */
    RPMERR_READERROR	= -111,	/*!< @todo Document. */
    RPMERR_UNKNOWNOS	= -112,	/*!< @todo Document. */
    RPMERR_UNKNOWNARCH	= -113,	/*!< @todo Document. */
    RPMERR_EXEC		= -114,	/*!< @todo Document. */
    RPMERR_FORK		= -115,	/*!< @todo Document. */
    RPMERR_CPIO		= -116,	/*!< @todo Document. */
    RPMERR_GZIP		= -117,	/*!< @todo Document. */
    RPMERR_BADSPEC	= -118,	/*!< @todo Document. */
    RPMERR_LDD		= -119,	/*!< couldn't understand ldd output */
    RPMERR_BADFILENAME	= -120,	/*!< @todo Document. */

    RPMERR_BADSIGTYPE	= -200,	/*!< Unknown signature type */
    RPMERR_SIGGEN	= -201	/*!< Error generating signature */
} rpmerrCode;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef void (*rpmErrorCallBackType)(void);

/**
 */
#if defined(__GNUC__)
void rpmError(rpmerrCode code, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
#else
void rpmError(rpmerrCode code, const char * format, ...);
#endif

/**
 * @todo Implement or eliminate.
 * @return		NULL always
 */
/*@observer@*/ /*@null@*/ const char * rpmErrorCodeString( /*@unused@*/ rpmerrCode code);

/**
 * @todo Implement or eliminate.
 */
typedef /*@abstract@*/ struct rpmerrRec_s {
    rpmerrCode	code;
    char	string[1024];
} * rpmerrRec;

/**
 * @return		error record code
 */
rpmerrCode rpmErrorCode(void);

/**
 * @return		error record string
 */
char * rpmErrorString(void);

/**
 */
rpmErrorCallBackType rpmErrorSetCallback(rpmErrorCallBackType);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMERR */
