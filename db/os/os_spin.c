/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997,2007 Oracle.  All rights reserved.
 *
 * $Id: os_spin.c,v 12.13 2007/05/17 15:15:46 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

#ifdef HAVE_SYSTEM_INCLUDE_FILES
#if defined(HAVE_PSTAT_GETDYNAMIC)
#include <sys/pstat.h>
#endif
#endif

#if defined(HAVE_PSTAT_GETDYNAMIC)
static int __os_pstat_getdynamic __P((void));

/*
 * __os_pstat_getdynamic --
 *	HP/UX.
 */
static int
__os_pstat_getdynamic()
{
	struct pst_dynamic psd;

	return (pstat_getdynamic(&psd,
	    sizeof(psd), (size_t)1, 0) == -1 ? 1 : psd.psd_proc_cnt);
}
#endif

#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
static u_int32_t __os_sysconf __P((void));

/*
 * __os_sysconf --
 *	Solaris, Linux.
 */
static u_int32_t
__os_sysconf()
{
	long nproc;

	nproc = sysconf(_SC_NPROCESSORS_ONLN);
	return ((u_int32_t)(nproc > 1 ? nproc : 1));
}
#endif

/*
 * __os_spin --
 *	Set the number of default spins before blocking.
 *
 * PUBLIC: u_int32_t __os_spin __P((DB_ENV *));
 */
u_int32_t
__os_spin(dbenv)
	DB_ENV *dbenv;
{
	u_int32_t tas_spins;

	COMPQUIET(dbenv, NULL);

	tas_spins = 1;
#if defined(HAVE_PSTAT_GETDYNAMIC)
	tas_spins = __os_pstat_getdynamic();
#endif
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
	tas_spins = __os_sysconf();
#endif

	/*
	 * Spin 50 times per processor, we have anecdotal evidence that this
	 * is a reasonable value.
	 */
	if (tas_spins != 1)
		tas_spins *= 50;

	return (tas_spins);
}
