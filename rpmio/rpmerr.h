#ifndef H_RPMERR
#define H_RPMERR

/** \ingroup rpmio
 * \file rpmio/rpmerr.h
 * @todo Eliminate from API.
 */

#include "rpmlog.h"

#define	_em(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_ERR))

/**
 * Tokens used by rpmError().
 */
typedef enum rpmerrCode_e {
    RPMERR_GDBMOPEN	= _em(2),   /*!< gdbm open failed */
    RPMERR_GDBMREAD	= _em(3),   /*!< gdbm read failed */
    RPMERR_GDBMWRITE	= _em(4),   /*!< gdbm write failed */
    RPMERR_INTERNAL	= _em(5),   /*!< internal RPM error */
    RPMERR_DBCORRUPT	= _em(6),   /*!< rpm database is corrupt */
    RPMERR_OLDDBCORRUPT	= _em(7),   /*!< old style rpm database is corrupt */
    RPMERR_OLDDBMISSING	= _em(8),   /*!< old style rpm database is missing */
    RPMERR_NOCREATEDB	= _em(9),   /*!< cannot create new database */
    RPMERR_DBOPEN	= _em(10),  /*!< database open failed */
    RPMERR_DBGETINDEX	= _em(11),  /*!< database get from index failed */
    RPMERR_DBPUTINDEX	= _em(12),  /*!< database get from index failed */
    RPMERR_NEWPACKAGE	= _em(13),  /*!< package is too new to handle */
    RPMERR_BADMAGIC	= _em(14),  /*!< bad magic for an RPM */
    RPMERR_RENAME	= _em(15),  /*!< rename(2) failed */
    RPMERR_UNLINK	= _em(16),  /*!< unlink(2) failed */
    RPMERR_RMDIR	= _em(17),  /*!< rmdir(2) failed */
    RPMERR_PKGINSTALLED	= _em(18),  /*!< package already installed */
    RPMERR_CHOWN	= _em(19),  /*!< chown() call failed */
    RPMERR_NOUSER	= _em(20),  /*!< user does not exist */
    RPMERR_NOGROUP	= _em(21),  /*!< group does not exist */
    RPMERR_MKDIR	= _em(22),  /*!< mkdir() call failed */
    RPMERR_FILECONFLICT	= _em(23),  /*!< file being installed exists */
    RPMERR_RPMRC	= _em(24),  /*!< bad line in rpmrc */
    RPMERR_NOSPEC	= _em(25),  /*!< .spec file is missing */
    RPMERR_NOTSRPM	= _em(26),  /*!< a source rpm was expected */
    RPMERR_FLOCK	= _em(27),  /*!< locking the database failed */
    RPMERR_OLDPACKAGE	= _em(28),  /*!< trying upgrading to old version */
/*    	RPMERR_BADARCH  = _em(29),  bad architecture or arch mismatch */
    RPMERR_CREATE	= _em(30),  /*!< failed to create a file */
    RPMERR_NOSPACE	= _em(31),  /*!< out of disk space */
    RPMERR_NORELOCATE	= _em(32),  /*!< tried to do improper relocatation */
/*    	RPMERR_BADOS    = _em(33),  bad architecture or arch mismatch */
    RPMMESS_BACKUP	= _em(34),  /*!< backup made during [un]install */
    RPMERR_MTAB		= _em(35),  /*!< failed to read mount table */
    RPMERR_STAT		= _em(36),  /*!< failed to stat something */
    RPMERR_BADDEV	= _em(37),  /*!< file on device not listed in mtab */
    RPMMESS_ALTNAME	= _em(38),  /*!< file written as .rpmnew */
    RPMMESS_PREREQLOOP	= _em(39),  /*!< loop in prerequisites */
    RPMERR_BADRELOCATE	= _em(40),  /*!< bad relocation was specified */
    RPMERR_OLDDB	= _em(41),  /*!< old format database */

/* spec.c build.c pack.c */
    RPMERR_UNMATCHEDIF	= _em(107), /*!< unclosed %ifarch or %ifos */
    RPMERR_BADARG	= _em(109), /*!< @todo Document. */
    RPMERR_SCRIPT	= _em(110), /*!< errors related to script exec */
    RPMERR_READ		= _em(111), /*!< @todo Document. */
    RPMERR_UNKNOWNOS	= _em(112), /*!< @todo Document. */
    RPMERR_UNKNOWNARCH	= _em(113), /*!< @todo Document. */
    RPMERR_EXEC		= _em(114), /*!< @todo Document. */
    RPMERR_FORK		= _em(115), /*!< @todo Document. */
    RPMERR_CPIO		= _em(116), /*!< @todo Document. */
    RPMERR_GZIP		= _em(117), /*!< @todo Document. */
    RPMERR_BADSPEC	= _em(118), /*!< @todo Document. */
    RPMERR_LDD		= _em(119), /*!< couldn't understand ldd output */
    RPMERR_BADFILENAME	= _em(120), /*!< @todo Document. */
    RPMERR_OPEN		= _em(121), /*!< @todo Document. */
    RPMERR_POPEN	= _em(122), /*!< @todo Document. */
    RPMERR_NOTREG	= _em(122), /*!< File %s is not a regular file */

    RPMERR_BADSIGTYPE	= _em(200), /*!< Unknown signature type */
    RPMERR_SIGGEN	= _em(201)  /*!< Error generating signature */
} rpmerrCode;

/**
 * Retrofit rpmError() onto rpmlog sub-system.
 */
#define	rpmError			rpmlog
#define	rpmErrorString()		rpmlogMessage()
#define	rpmErrorSetCallback(_cb)	rpmlogSetCallback(_cb)
typedef rpmlogCallback rpmErrorCallBackType;


#endif  /* H_RPMERR */
