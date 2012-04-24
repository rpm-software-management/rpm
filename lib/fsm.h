#ifndef H_FSM
#define H_FSM

/** \ingroup payload
 * \file lib/fsm.h
 * File state machine to handle a payload within an rpm package.
 */

#include <rpm/rpmfi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpmpsm_s * rpmpsm;

/**
 * Execute a file actions for package
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @param cfd
 * @param psm		owner psm (or NULL)
 * @retval archiveSize	pointer to archive size
 * @retval failedFile	pointer to first file name that failed (malloced)
 * @return		0 on success
 */

int rpmPackageFilesInstall(rpmts ts, rpmte te, rpmfi fi, FD_t cfd,
              rpmpsm psm, char ** failedFile);

int rpmPackageFilesRemove(rpmts ts, rpmte te, rpmfi fi,
              rpmpsm psm, char ** failedFile);

int rpmPackageFilesArchive(rpmfi fi, int isSrc, FD_t cfd,
              rpm_loff_t * archiveSize, char ** failedFile);

RPM_GNUC_INTERNAL
void rpmpsmNotify(rpmpsm psm, int what, rpm_loff_t amount);
#ifdef __cplusplus
}
#endif

#endif	/* H_FSM */
