#ifndef H_PSM
#define H_PSM

/** \ingroup rpmtrans payload
 * \file lib/psm.h
 * Package state machine to handle a package from a transaction set.
 */

#include "fsm.h"
#include "depends.h"

/**
 */
#define	PSM_VERBOSE	0x8000
#define	PSM_INTERNAL	0x4000
#define	PSM_SYSCALL	0x2000
#define	PSM_DEAD	0x1000
#define	_fv(_a)		((_a) | PSM_VERBOSE)
#define	_fi(_a)		((_a) | PSM_INTERNAL)
#define	_fs(_a)		((_a) | (PSM_INTERNAL | PSM_SYSCALL))
#define	_fd(_a)		((_a) | (PSM_INTERNAL | PSM_DEAD))
typedef enum pkgStage_e {
    PSM_UNKNOWN		=  0,
    PSM_INIT		=  1,
    PSM_PRE		=  2,
    PSM_PROCESS		=  3,
    PSM_POST		=  4,
    PSM_UNDO		=  5,
    PSM_FINI		=  6,

    PSM_PKGINSTALL	=  7,
    PSM_PKGERASE	=  8,
    PSM_PKGCOMMIT	= 10,
    PSM_PKGSAVE		= 12,

    PSM_CREATE		= 17,
    PSM_NOTIFY		= 22,
    PSM_DESTROY		= 23,
    PSM_COMMIT		= 25,

    PSM_CHROOT_IN	= 51,
    PSM_CHROOT_OUT	= 52,
    PSM_SCRIPT		= 53,
    PSM_TRIGGERS	= 54,
    PSM_IMMED_TRIGGERS	= 55,
    PSM_RPMIO_FLAGS	= 56,

    PSM_RPMDB_LOAD	= 97,
    PSM_RPMDB_ADD	= 98,
    PSM_RPMDB_REMOVE	= 99,

} pkgStage;
#undef	_fv
#undef	_fi
#undef	_fs
#undef	_fd

/**
 */
struct psm_s {
/*@refcounted@*/
    rpmTransactionSet ts;	/*!< transaction set */
/*@dependent@*/
    transactionElement te;	/*!< transaction element */
/*@refcounted@*/
    TFI_t fi;			/*!< transaction element file info */
    FD_t cfd;			/*!< Payload file handle. */
    FD_t fd;			/*!< Repackage file handle. */
    Header oh;			/*!< Repackage/multilib header. */
/*@null@*/
    rpmdbMatchIterator mi;
/*@observer@*/
    const char * stepName;
/*@only@*/ /*@null@*/
    const char * rpmio_flags;
/*@only@*/ /*@null@*/
    const char * failedFile;
/*@only@*/ /*@null@*/
    const char * pkgURL;	/*!< Repackage URL. */
/*@dependent@*/
    const char * pkgfn;		/*!< Repackage file name. */
    int scriptTag;		/*!< Scriptlet data tag. */
    int progTag;		/*!< Scriptlet interpreter tag. */
    int npkgs_installed;	/*!< No. of installed instances. */
    int scriptArg;		/*!< Scriptlet package arg. */
    int sense;			/*!< One of RPMSENSE_TRIGGER{IN,UN,POSTUN}. */
    int countCorrection;	/*!< 0 if installing, -1 if removing. */
    int chrootDone;		/*!< Was chroot(2) done by pkgStage? */
    rpmCallbackType what;	/*!< Callback type. */
    unsigned long amount;	/*!< Callback amount. */
    unsigned long total;	/*!< Callback total. */
    rpmRC rc;
    pkgStage goal;
/*@unused@*/
    pkgStage stage;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return formatted string representation of package disposition.
 * @param a		package dispostion
 * @return		formatted string
 */
/*@observer@*/ const char *const fiTypeString(/*@partial@*/TFI_t fi)
	/*@*/;

/**
 * Package state machine driver.
 * @param psm		package state machine data
 * @param stage		next stage
 * @return		0 on success
 */
int psmStage(PSM_t psm, pkgStage stage)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies psm, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
