#ifndef H_PSM
#define H_PSM

/** \ingroup rpmtrans payload
 * \file lib/psm.h
 * Package state machine to handle a package from a transaction set.
 */

#define _RPMSQ_INTERNAL 
#include <rpm/rpmsq.h>
#include <rpm/rpmcallback.h>

extern int _psm_debug;

typedef struct rpmpsm_s * rpmpsm;

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		NULL always
 */
rpmpsm rpmpsmUnlink (rpmpsm psm,
		const char * msg);

/**
 * Reference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		new package state machine reference
 */
rpmpsm rpmpsmLink (rpmpsm psm, const char * msg);

/**
 * Destroy a package state machine.
 * @param psm		package state machine
 * @return		NULL always
 */
rpmpsm rpmpsmFree(rpmpsm psm);

/**
 * Create and load a package state machine.
 * @param ts		transaction set
 * @param te		transaction set element
 * @param fi		file info set
 * @return		new package state machine
 */
rpmpsm rpmpsmNew(rpmts ts, rpmte te, rpmfi fi);

/**
 * Package state machine driver.
 * @param psm		package state machine data
 * @param stage		next stage
 * @return		0 on success
 */
rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage);
#define	rpmpsmUNSAFE	rpmpsmSTAGE

/**
 * Run rpmpsmStage(PSM_SCRIPT) for scriptTag and progTag
 * @param psm		package state machine data
 * @param scriptTag	scriptlet tag to execute
 * @param progTag	scriptlet prog tag to execute
 * @return 		0 on success
 */
rpmRC rpmpsmScriptStage(rpmpsm psm, rpmTag scriptTag, rpmTag progTag);

/**
 * @param psm		package state machine data
 * @param fi		new file info pointer (or NULL to dealloc)
 * @return		newly set rpmfi pointer
 */
rpmfi rpmpsmSetFI(rpmpsm psm, rpmfi fi);

/**
 * @param psm		package state machine data
 * @return 		psm transaction set pointer
 */
rpmts rpmpsmGetTs(rpmpsm psm);

void rpmpsmSetAsync(rpmpsm psm, int async);

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
