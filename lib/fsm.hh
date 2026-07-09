#ifndef H_FSM
#define H_FSM

/** \ingroup payload
 * \file fsm.h
 * File state machine to handle a payload within an rpm package.
 */

#include <rpm/rpmfi.h>
#include <rpm/rpmcallback.h>

typedef struct rpmpsm_s * rpmpsm;

/**
 * Execute a file actions for package
 * @param ts		transaction set
 * @param te		transaction set element
 * @param files		transaction element file info
 * @param psm		owner psm (or NULL)
 * @param[out] failedFile	pointer to first file name that failed (malloced)
 * @return		0 on success
 */

RPM_GNUC_INTERNAL
int rpmPackageFilesInstall(rpmts ts, rpmte te, rpmfiles files,
              rpmpsm psm, char ** failedFile);

RPM_GNUC_INTERNAL
int rpmPackageFilesRemove(rpmts ts, rpmte te, rpmfiles files,
              rpmpsm psm, char ** failedFile);

RPM_GNUC_INTERNAL
int rpmfiArchiveReadToFilePsm(rpmfi fi, FD_t fd, int nodigest, rpmpsm psm);

/*
 * Copy the current archive file's content straight out of the (uncompressed)
 * package with a reflink or copy_file_range(). src_fd is a raw descriptor for
 * the package, base its payload start offset. Verified installs read the
 * destination back through its O_RDWR descriptor and compare the file digest.
 * Returns 0 on success,
 * RPMCPIO_COPY_FALLBACK if the content cannot be copied that way (caller should
 * use rpmfiArchiveReadToFilePsm), < 0 on hard error.
 */
RPM_GNUC_INTERNAL
int rpmfiArchiveWriteFileTo(rpmfi fi, int dst_fd, int src_fd,
			    rpm_loff_t base, int nodigest, rpmpsm psm);

RPM_GNUC_INTERNAL
void rpmpsmNotify(rpmpsm psm, rpmCallbackType what, rpm_loff_t amount);

#endif	/* H_FSM */
