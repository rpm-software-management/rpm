#ifndef _RPMFILES_H
#define _RPMFILES_H

/** \ingroup rpmfiles
 * \file lib/rpmfiles.h
 * File info set API.
 */
#include <rpm/rpmtypes.h>
#include <rpm/rpmvf.h>
#include <rpm/rpmpgp.h>

/** \ingroup rpmfi
 * File types.
 * These are the file types used internally by rpm. The file
 * type is determined by applying stat(2) macros like S_ISDIR to
 * the file mode tag from a header. The values are arbitrary,
 * but are identical to the linux stat(2) file types.
 */
typedef enum rpmFileTypes_e {
    PIPE	=  1,	/*!< pipe/fifo */
    CDEV	=  2,	/*!< character device */
    XDIR	=  4,	/*!< directory */
    BDEV	=  6,	/*!< block device */
    REG		=  8,	/*!< regular file */
    LINK	= 10,	/*!< hard link */
    SOCK	= 12	/*!< socket */
} rpmFileTypes;

/**
 * File States (when installed).
 */
typedef enum rpmfileState_e {
    RPMFILE_STATE_MISSING	= -1,	/* used for unavailable data */
    RPMFILE_STATE_NORMAL 	= 0,
    RPMFILE_STATE_REPLACED 	= 1,
    RPMFILE_STATE_NOTINSTALLED	= 2,
    RPMFILE_STATE_NETSHARED	= 3,
    RPMFILE_STATE_WRONGCOLOR	= 4
} rpmfileState;

#define RPMFILE_IS_INSTALLED(_x) ((_x) == RPMFILE_STATE_NORMAL || (_x) == RPMFILE_STATE_NETSHARED)

/**
 * Exported File Attributes (ie RPMTAG_FILEFLAGS)
 */
enum rpmfileAttrs_e {
    RPMFILE_NONE	= 0,
    RPMFILE_CONFIG	= (1 <<  0),	/*!< from %%config */
    RPMFILE_DOC		= (1 <<  1),	/*!< from %%doc */
    RPMFILE_ICON	= (1 <<  2),	/*!< from %%donotuse. */
    RPMFILE_MISSINGOK	= (1 <<  3),	/*!< from %%config(missingok) */
    RPMFILE_NOREPLACE	= (1 <<  4),	/*!< from %%config(noreplace) */
    RPMFILE_SPECFILE	= (1 <<  5),	/*!< @todo (unnecessary) marks 1st file in srpm. */
    RPMFILE_GHOST	= (1 <<  6),	/*!< from %%ghost */
    RPMFILE_LICENSE	= (1 <<  7),	/*!< from %%license */
    RPMFILE_README	= (1 <<  8),	/*!< from %%readme */
    /* bits 9-10 unused */
    RPMFILE_PUBKEY	= (1 << 11),	/*!< from %%pubkey */
};

typedef rpmFlags rpmfileAttrs;

#define	RPMFILE_ALL	~(RPMFILE_NONE)

/** \ingroup rpmfi
 * File disposition(s) during package install/erase transaction.
 */
typedef enum rpmFileAction_e {
    FA_UNKNOWN		= 0,	/*!< initial action for file ... */
    FA_CREATE		= 1,	/*!< ... create from payload. */
    FA_COPYIN		= 2,	/*!< obsolete, unused. */
    FA_COPYOUT		= 3,	/*!< obsolete, unused. */
    FA_BACKUP		= 4,	/*!< ... renamed with ".rpmorig" extension. */
    FA_SAVE		= 5,	/*!< ... renamed with ".rpmsave" extension. */
    FA_SKIP		= 6, 	/*!< ... already replaced, don't remove. */
    FA_ALTNAME		= 7,	/*!< ... create with ".rpmnew" extension. */
    FA_ERASE		= 8,	/*!< ... to be removed. */
    FA_SKIPNSTATE	= 9,	/*!< ... untouched, state "not installed". */
    FA_SKIPNETSHARED	= 10,	/*!< ... untouched, state "netshared". */
    FA_SKIPCOLOR	= 11,	/*!< ... untouched, state "wrong color". */
    /* bits 16-31 reserved */
} rpmFileAction;

#define XFA_SKIPPING(_a)	\
    ((_a) == FA_SKIP || (_a) == FA_SKIPNSTATE || (_a) == FA_SKIPNETSHARED || (_a) == FA_SKIPCOLOR)

/**
 * We pass these around as an array with a sentinel.
 */
struct rpmRelocation_s {
    char * oldPath;	/*!< NULL here evals to RPMTAG_DEFAULTPREFIX, */
    char * newPath;	/*!< NULL means to omit the file completely! */
};

enum rpmfiFlags_e {
    RPMFI_NOHEADER		= 0,
    RPMFI_KEEPHEADER		= (1 << 0),
    RPMFI_NOFILECLASS		= (1 << 1),
    RPMFI_NOFILEDEPS		= (1 << 2),
    RPMFI_NOFILELANGS		= (1 << 3),
    RPMFI_NOFILEUSER		= (1 << 4),
    RPMFI_NOFILEGROUP		= (1 << 5),
    RPMFI_NOFILEMODES		= (1 << 6),
    RPMFI_NOFILESIZES		= (1 << 7),
    RPMFI_NOFILECAPS		= (1 << 8),
    RPMFI_NOFILELINKTOS		= (1 << 9),
    RPMFI_NOFILEDIGESTS		= (1 << 10),
    RPMFI_NOFILEMTIMES		= (1 << 11),
    RPMFI_NOFILERDEVS		= (1 << 12),
    RPMFI_NOFILEINODES		= (1 << 13),
    RPMFI_NOFILESTATES		= (1 << 14),
    RPMFI_NOFILECOLORS		= (1 << 15),
    RPMFI_NOFILEVERIFYFLAGS	= (1 << 16),
    RPMFI_NOFILEFLAGS		= (1 << 17),
};

typedef rpmFlags rpmfiFlags;

#define RPMFI_FLAGS_ERASE \
    (RPMFI_NOFILECLASS | RPMFI_NOFILELANGS | \
     RPMFI_NOFILEMTIMES | RPMFI_NOFILERDEVS | RPMFI_NOFILEINODES | \
     RPMFI_NOFILEVERIFYFLAGS)

#define RPMFI_FLAGS_INSTALL \
    (RPMFI_NOFILECLASS | RPMFI_NOFILEVERIFYFLAGS)

#define RPMFI_FLAGS_VERIFY \
    (RPMFI_NOFILECLASS | RPMFI_NOFILEDEPS | RPMFI_NOFILELANGS | \
     RPMFI_NOFILECOLORS)

#define RPMFI_FLAGS_QUERY \
    (RPMFI_NOFILECLASS | RPMFI_NOFILEDEPS | RPMFI_NOFILELANGS | \
     RPMFI_NOFILECOLORS | RPMFI_NOFILEVERIFYFLAGS)

typedef enum rpmFileIter_e {
    RPMFI_ITER_FWD	= 0,
    RPMFI_ITER_BACK	= 1,
    RPMFI_ITER_READ_ARCHIVE	= 2,
    RPMFI_ITER_WRITE_ARCHIVE	= 3,
} rpmFileIter;

#ifdef __cplusplus
extern "C" {
#endif

rpmfiles rpmfilesNew(rpmstrPool pool, Header h, rpmTagVal tagN, rpmfiFlags flags);

rpmfiles rpmfilesLink(rpmfiles fi);

rpmfiles rpmfilesFree(rpmfiles fi);

rpm_count_t rpmfilesFC(rpmfiles fi);

rpm_count_t rpmfilesDC(rpmfiles fi);

int rpmfilesFindFN(rpmfiles files, const char * fn);

int rpmfilesFindOFN(rpmfiles files, const char * fn);

rpmfi rpmfilesIter(rpmfiles files, int itype);

int rpmfilesDigestAlgo(rpmfiles fi);

rpm_color_t rpmfilesColor(rpmfiles files);

int rpmfilesCompare(rpmfiles afi, int aix, rpmfiles bfi, int bix);

const char * rpmfilesBN(rpmfiles fi, int ix);

const char * rpmfilesDN(rpmfiles fi, int jx);

int rpmfilesDI(rpmfiles fi, int dx);

char * rpmfilesFN(rpmfiles fi, int ix);

int rpmfilesODI(rpmfiles fi, int dx);

const char * rpmfilesOBN(rpmfiles fi, int ix);

const char * rpmfilesODN(rpmfiles fi, int jx);

char * rpmfilesOFN(rpmfiles fi, int ix);

rpmVerifyAttrs rpmfilesVFlags(rpmfiles fi, int ix);

rpmfileState rpmfilesFState(rpmfiles fi, int ix);

const char * rpmfilesFLink(rpmfiles fi, int ix);

rpm_loff_t rpmfilesFSize(rpmfiles fi, int ix);

rpm_color_t rpmfilesFColor(rpmfiles fi, int ix);

const char * rpmfilesFClass(rpmfiles fi, int ix);

uint32_t rpmfilesFDepends(rpmfiles fi, int ix, const uint32_t ** fddictp);

uint32_t rpmfilesFNlink(rpmfiles fi, int ix);

uint32_t rpmfilesFLinks(rpmfiles fi, int ix, const int ** files);

const char * rpmfilesFLangs(rpmfiles fi, int ix);

rpmfileAttrs rpmfilesFFlags(rpmfiles fi, int ix);

rpm_mode_t rpmfilesFMode(rpmfiles fi, int ix);

const unsigned char * rpmfilesFDigest(rpmfiles fi, int ix, int *algo, size_t *len);

rpm_rdev_t rpmfilesFRdev(rpmfiles fi, int ix);

rpm_ino_t rpmfilesFInode(rpmfiles fi, int ix);

rpm_time_t rpmfilesFMtime(rpmfiles fi, int ix);

const char * rpmfilesFUser(rpmfiles fi, int ix);

const char * rpmfilesFGroup(rpmfiles fi, int ix);

const char * rpmfilesFCaps(rpmfiles fi, int ix);

#ifdef __cplusplus
}
#endif

#endif /* _RPMFILES_H */
