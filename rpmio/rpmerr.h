#ifndef H_RPMERR
#define H_RPMERR

/** \ingroup rpmio
 * \file rpmio/rpmerr.h
 * @todo Eliminate from API.
 */

#include "rpmlog.h"

#define	_em(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_ERR))
#define	_wm(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_WARNING))
#define	_nm(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_NOTICE))
#define	_im(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_INFO))
#define	_dm(_e)	\
    (((_e) << 16) | RPMLOG_MAKEPRI(RPMLOG_ERRMSG, RPMLOG_DEBUG))

/**
 * Tokens used by rpmError().
 */
/*@-typeuse @*/
typedef enum rpmerrCode_e {
/*@-enummemuse@*/
    RPMERR_GDBMOPEN	= _em(2),   /*!< gdbm open failed */
    RPMERR_GDBMREAD	= _em(3),   /*!< gdbm read failed */
    RPMERR_GDBMWRITE	= _em(4),   /*!< gdbm write failed */
/*@=enummemuse@*/
    RPMERR_INTERNAL	= _em(5),   /*!< internal RPM error */
    RPMERR_DBCORRUPT	= _em(6),   /*!< rpm database is corrupt */
/*@-enummemuse@*/
    RPMERR_OLDDBCORRUPT	= _em(7),   /*!< old style rpm database is corrupt */
    RPMERR_OLDDBMISSING	= _em(8),   /*!< old style rpm database is missing */
    RPMERR_NOCREATEDB	= _em(9),   /*!< cannot create new database */
/*@=enummemuse@*/
    RPMERR_DBOPEN	= _em(10),  /*!< database open failed */
    RPMERR_DBGETINDEX	= _em(11),  /*!< database get from index failed */
    RPMERR_DBPUTINDEX	= _em(12),  /*!< database get from index failed */
    RPMERR_NEWPACKAGE	= _em(13),  /*!< package is too new to handle */
    RPMERR_BADMAGIC	= _em(14),  /*!< bad magic for an RPM */
    RPMERR_RENAME	= _em(15),  /*!< rename(2) failed */
    RPMERR_UNLINK	= _em(16),  /*!< unlink(2) failed */
    RPMERR_RMDIR	= _em(17),  /*!< rmdir(2) failed */
/*@-enummemuse@*/
    RPMERR_PKGINSTALLED	= _em(18),  /*!< package already installed */
    RPMERR_CHOWN	= _em(19),  /*!< chown() call failed */
    RPMERR_NOUSER	= _em(20),  /*!< user does not exist */
    RPMERR_NOGROUP	= _em(21),  /*!< group does not exist */
/*@=enummemuse@*/
    RPMERR_MKDIR	= _em(22),  /*!< mkdir() call failed */
/*@-enummemuse@*/
    RPMERR_FILECONFLICT	= _em(23),  /*!< file being installed exists */
/*@=enummemuse@*/
    RPMERR_RPMRC	= _em(24),  /*!< bad line in rpmrc */
    RPMERR_NOSPEC	= _em(25),  /*!< .spec file is missing */
    RPMERR_NOTSRPM	= _em(26),  /*!< a source rpm was expected */
    RPMERR_FLOCK	= _em(27),  /*!< locking the database failed */
/*@-enummemuse@*/
    RPMERR_OLDPACKAGE	= _em(28),  /*!< trying upgrading to old version */
/*    	RPMERR_BADARCH  = _em(29),  bad architecture or arch mismatch */
/*@=enummemuse@*/
    RPMERR_CREATE	= _em(30),  /*!< failed to create a file */
    RPMERR_NOSPACE	= _em(31),  /*!< out of disk space */
/*@-enummemuse@*/
    RPMERR_NORELOCATE	= _em(32),  /*!< tried to do improper relocatation */
/*    	RPMERR_BADOS    = _em(33),  bad architecture or arch mismatch */
    RPMMESS_BACKUP	= _em(34),  /*!< backup made during [un]install */
/*@=enummemuse@*/
    RPMERR_MTAB		= _em(35),  /*!< failed to read mount table */
    RPMERR_STAT		= _em(36),  /*!< failed to stat something */
    RPMERR_BADDEV	= _em(37),  /*!< file on device not listed in mtab */
/*@-enummemuse@*/
    RPMMESS_ALTNAME	= _em(38),  /*!< file written as .rpmnew */
    RPMMESS_PREREQLOOP	= _em(39),  /*!< loop in prerequisites */
    RPMERR_BADRELOCATE	= _em(40),  /*!< bad relocation was specified */
/*@=enummemuse@*/
    RPMERR_OLDDB	= _em(41),  /*!< old format database */

    RPMERR_UNMATCHEDIF	= _em(107), /*!< unclosed %ifarch or %ifos */
    RPMERR_RELOAD	= _em(108), /*!< */
    RPMERR_BADARG	= _em(109), /*!< */
    RPMERR_SCRIPT	= _em(110), /*!< errors related to script exec */
    RPMERR_READ		= _em(111), /*!< */
/*@-enummemuse@*/
    RPMERR_UNKNOWNOS	= _em(112), /*!< */
    RPMERR_UNKNOWNARCH	= _em(113), /*!< */
/*@=enummemuse@*/
    RPMERR_EXEC		= _em(114), /*!< */
    RPMERR_FORK		= _em(115), /*!< */
    RPMERR_CPIO		= _em(116), /*!< */
/*@-enummemuse@*/
    RPMERR_GZIP		= _em(117), /*!< */
/*@=enummemuse@*/
    RPMERR_BADSPEC	= _em(118), /*!< */
/*@-enummemuse@*/
    RPMERR_LDD		= _em(119), /*!< couldn't understand ldd output */
/*@=enummemuse@*/
    RPMERR_BADFILENAME	= _em(120), /*!< */
    RPMERR_OPEN		= _em(121), /*!< */
    RPMERR_POPEN	= _em(122), /*!< */
    RPMERR_NOTREG	= _em(123), /*!< File %s is not a regular file */
    RPMERR_QUERY	= _em(124), /*!< */
    RPMERR_QFMT		= _em(125), /*!< */
    RPMERR_DBCONFIG	= _em(126), /*!< */
    RPMERR_DBERR	= _em(127), /*!< */
/*@-enummemuse@*/
    RPMERR_BADPACKAGE	= _em(128), /*!< getNextHeader: %s */
/*@=enummemuse@*/
    RPMERR_FREELIST	= _em(129), /*!< free list corrupt (%u)- please ... */
    RPMERR_DATATYPE	= _em(130), /*!< Data type %d not supported */
    RPMERR_BUILDROOT	= _em(131), /*!< */
    RPMERR_MAKETEMP	= _em(132), /*!< makeTempFile failed */
    RPMERR_FWRITE	= _em(133), /*!< %s: Fwrite failed: %s */
    RPMERR_FREAD	= _em(134), /*!< %s: Fread failed: %s */
    RPMERR_READLEAD	= _em(135), /*!< %s: readLead failed */
    RPMERR_WRITELEAD	= _em(136), /*!< %s: writeLead failed: %s */
    RPMERR_QUERYINFO	= _nm(137), /*!< */
    RPMERR_MANIFEST	= _nm(138), /*!< %s: read manifest failed: %s */
    RPMERR_BADHEADER	= _em(139), /*!< */
    RPMERR_FSEEK	= _em(140), /*!< %s: Fseek failed: %s */
    RPMERR_REGCOMP	= _em(141), /*!< %s: regcomp failed: %s */
    RPMERR_REGEXEC	= _em(142), /*!< %s: regexec failed: %s */

    RPMERR_BADSIGTYPE	= _em(200), /*!< Unknown signature type */
    RPMERR_SIGGEN	= _em(201), /*!< Error generating signature */
    RPMERR_SIGVFY	= _nm(202), /*!< */

    RPMWARN_UNLINK	= _wm(512u+16),  /*!< unlink(2) failed */
    RPMWARN_RMDIR	= _wm(512u+17),  /*!< rmdir(2) failed */
    RPMWARN_FLOCK	= _wm(512u+27)   /*!< locking the database failed */
} rpmerrCode;
/*@=typeuse @*/

/**
 * Retrofit rpmError() onto rpmlog sub-system.
 */
#define	rpmError			rpmlog
#define	rpmErrorCode()			rpmlogCode()
#define	rpmErrorString()		rpmlogMessage()
#define	rpmErrorSetCallback(_cb)	rpmlogSetCallback(_cb)
/*@-typeuse@*/
typedef rpmlogCallback rpmErrorCallBackType;
/*@=typeuse@*/


#endif  /* H_RPMERR */
