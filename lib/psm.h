#ifndef H_PSM
#define H_PSM

/** \ingroup rpmtrans payload
 * \file lib/psm.h
 * Package state machine to handle a package from a transaction set.
 */

/*@-exportlocal@*/
/*@unchecked@*/
extern int _psm_debug;
/*@=exportlocal@*/

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
    PSM_RPMDB_REMOVE	= 99

} pkgStage;
#undef	_fv
#undef	_fi
#undef	_fs
#undef	_fd

/**
 */
struct rpmpsm_s {
/*@refcounted@*/
    rpmts ts;			/*!< transaction set */
/*@dependent@*/ /*@null@*/
    rpmte te;			/*!< current transaction element */
/*@refcounted@*/
    rpmfi fi;			/*!< transaction element file info */
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
    int unorderedSuccessor;	/*!< Can the PSM be run asynchronously? */
    int reaper;			/*!< Register SIGCHLD handler? */
    pid_t reaped;		/*!< Reaped waitpid return. */
    pid_t child;		/*!< Currently running process. */
    int status;			/*!< Reaped waitpid status. */
    rpmCallbackType what;	/*!< Callback type. */
    unsigned long amount;	/*!< Callback amount. */
    unsigned long total;	/*!< Callback total. */
    rpmRC rc;
    pkgStage goal;
/*@unused@*/
    pkgStage stage;

/*@refs@*/
    int nrefs;			/*!< Reference count. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmpsm rpmpsmUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmpsm psm,
		/*@null@*/ const char * msg)
	/*@modifies psm @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmpsm XrpmpsmUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmpsm psm,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies psm @*/;
/*@=exportlocal@*/
#define	rpmpsmUnlink(_psm, _msg)	XrpmpsmUnlink(_psm, _msg, __FILE__, __LINE__)

/**
 * Reference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		new package state machine reference
 */
/*@unused@*/ /*@newref@*/
rpmpsm rpmpsmLink (/*@null@*/ rpmpsm psm, /*@null@*/ const char * msg)
	/*@modifies psm @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@newref@*/
rpmpsm XrpmpsmLink (/*@null@*/ rpmpsm psm, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies psm @*/;
/*@=exportlocal@*/
#define	rpmpsmLink(_psm, _msg)	XrpmpsmLink(_psm, _msg, __FILE__, __LINE__)

/**
 * Destroy a package state machine.
 * @param psm		package state machine
 * @return		NULL always
 */
/*@null@*/
rpmpsm rpmpsmFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmpsm psm)
	/*@globals fileSystem @*/
	/*@modifies psm, fileSystem @*/;

/**
 * Create and load a package state machine.
 * @param ts		transaction set
 * @param te		transaction set element
 * @param fi		file info set
 * @return		new package state machine
 */
rpmpsm rpmpsmNew(rpmts ts, /*@null@*/ rpmte te, rpmfi fi)
	/*@modifies ts, fi @*/;

/**
 * Package state machine driver.
 * @param psm		package state machine data
 * @param stage		next stage
 * @return		0 on success
 */
int rpmpsmStage(rpmpsm psm, pkgStage stage)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies psm, rpmGlobalMacroContext, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
