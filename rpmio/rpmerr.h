#ifndef H_RPMERR
#define H_RPMERR

#define	RPMERR_GDBMOPEN		-2      /* gdbm open failed */
#define	RPMERR_GDBMREAD		-3	/* gdbm read failed */
#define	RPMERR_GDBMWRITE	-4	/* gdbm write failed */
#define	RPMERR_INTERNAL		-5	/* internal RPM error */
#define	RPMERR_DBCORRUPT	-6	/* rpm database is corrupt */
#define	RPMERR_OLDDBCORRUPT	-7	/* old style rpm database is corrupt */
#define	RPMERR_OLDDBMISSING	-8	/* old style rpm database is missing */
#define	RPMERR_NOCREATEDB	-9	/* cannot create new database */
#define	RPMERR_DBOPEN		-10     /* database open failed */
#define	RPMERR_DBGETINDEX	-11     /* database get from index failed */
#define	RPMERR_DBPUTINDEX	-12     /* database get from index failed */
#define	RPMERR_NEWPACKAGE	-13     /* package is too new to handle */
#define	RPMERR_BADMAGIC		-14	/* bad magic for an RPM */
#define	RPMERR_RENAME		-15	/* rename(2) failed */
#define	RPMERR_UNLINK		-16	/* unlink(2) failed */
#define	RPMERR_RMDIR		-17	/* rmdir(2) failed */
#define	RPMERR_PKGINSTALLED	-18	/* package already installed */
#define	RPMERR_CHOWN		-19	/* chown() call failed */
#define	RPMERR_NOUSER		-20	/* user does not exist */
#define	RPMERR_NOGROUP		-21	/* group does not exist */
#define	RPMERR_MKDIR		-22	/* mkdir() call failed */
#define	RPMERR_FILECONFLICT     -23     /* file being installed exists */
#define	RPMERR_RPMRC		-24     /* bad line in rpmrc */
#define	RPMERR_NOSPEC		-25     /* .spec file is missing */
#define	RPMERR_NOTSRPM		-26     /* a source rpm was expected */
#define	RPMERR_FLOCK		-27     /* locking the database failed */
#define	RPMERR_OLDPACKAGE	-28	/* trying upgrading to old version */
/*#define	RPMERR_BADARCH  -29        bad architecture or arch mismatch */
#define	RPMERR_CREATE		-30	/* failed to create a file */
#define	RPMERR_NOSPACE		-31	/* out of disk space */
#define	RPMERR_NORELOCATE	-32	/* tried to do improper relocatation */
/*#define	RPMERR_BADOS    -33        bad architecture or arch mismatch */
#define	RPMMESS_BACKUP          -34     /* backup made during [un]install */
#define	RPMERR_MTAB		-35	/* failed to read mount table */
#define	RPMERR_STAT		-36	/* failed to stat something */
#define	RPMERR_BADDEV		-37	/* file on device not listed in mtab */
#define	RPMMESS_ALTNAME         -38     /* file written as .rpmnew */
#define	RPMMESS_PREREQLOOP      -39     /* loop in prerequisites */
#define	RPMERR_BADRELOCATE      -40     /* bad relocation was specified */
#define	RPMERR_OLDDB      	-41     /* old format database */

/* spec.c build.c pack.c */
#define	RPMERR_UNMATCHEDIF      -107    /* unclosed %ifarch or %ifos */
#define	RPMERR_BADARG           -109
#define	RPMERR_SCRIPT           -110    /* errors related to script exec */
#define	RPMERR_READERROR        -111
#define	RPMERR_UNKNOWNOS        -112
#define	RPMERR_UNKNOWNARCH      -113
#define	RPMERR_EXEC             -114
#define	RPMERR_FORK             -115
#define	RPMERR_CPIO             -116
#define	RPMERR_GZIP             -117
#define	RPMERR_BADSPEC          -118
#define	RPMERR_LDD              -119    /* couldn't understand ldd output */
#define	RPMERR_BADFILENAME	-120

#define	RPMERR_BADSIGTYPE       -200    /* Unknown signature type */
#define	RPMERR_SIGGEN           -201    /* Error generating signature */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rpmErrorCallBackType)(void);

#if defined(__GNUC__)
void rpmError(int code, char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
#else
void rpmError(int code, char * format, ...);
#endif

int rpmErrorCode(void);
char *rpmErrorCodeString(void);
char *rpmErrorString(void);
rpmErrorCallBackType rpmErrorSetCallback(rpmErrorCallBackType);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMERR */
