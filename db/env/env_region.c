/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: env_region.c,v 12.35 2007/06/05 13:25:26 ubell Exp $
 */

#include "db_config.h"

#include "db_int.h"
#include "dbinc/mp.h"

static void __env_des_destroy __P((DB_ENV *, REGION *));
static int  __env_des_get __P((DB_ENV *, REGINFO *, REGINFO *, REGION **));
static int  __env_faultmem __P((DB_ENV *, void *, size_t, int));
static void __env_remove_file __P((DB_ENV *));

/*
 * __env_attach
 *	Join/create the environment
 *
 * PUBLIC: int __env_attach __P((DB_ENV *, u_int32_t *, int, int));
 */
int
__env_attach(dbenv, init_flagsp, create_ok, retry_ok)
	DB_ENV *dbenv;
	u_int32_t *init_flagsp;
	int create_ok, retry_ok;
{
	REGENV *renv;
	REGENV_REF ref;
	REGINFO *infop;
	REGION *rp, tregion;
	size_t nrw, s, size;
	u_int32_t bytes, i, mbytes, nregions;
	u_int retry_cnt;
	int majver, minver, patchver, ret, segid;
	char buf[sizeof(DB_REGION_FMT) + 20];

	/* Initialization */
	retry_cnt = 0;

	/* Repeated initialization. */
loop:	renv = NULL;

	/* Set up the DB_ENV's REG_INFO structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(REGINFO), &infop)) != 0)
		return (ret);
	infop->dbenv = dbenv;
	infop->type = REGION_TYPE_ENV;
	infop->id = REGION_ID_ENV;
	infop->flags = REGION_JOIN_OK;
	if (create_ok)
		F_SET(infop, REGION_CREATE_OK);

	/*
	 * We have to single-thread the creation of the REGENV region.  Once
	 * it exists, we can serialize using region mutexes, but until then
	 * we have to be the only player in the game.
	 *
	 * If this is a private environment, we are only called once and there
	 * are no possible race conditions.
	 *
	 * If this is a public environment, we use the filesystem to ensure
	 * the creation of the environment file is single-threaded.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		if ((ret = __os_strdup(dbenv,
		    "process-private", &infop->name)) != 0)
			goto err;
		goto creation;
	}

	/* Build the region name. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, buf, 0, NULL, &infop->name)) != 0)
		goto err;

	/*
	 * Try to create the file, if we have the authority.  We have to ensure
	 * that multiple threads/processes attempting to simultaneously create
	 * the file are properly ordered.  Open using the O_CREAT and O_EXCL
	 * flags so that multiple attempts to create the region will return
	 * failure in all but one.  POSIX 1003.1 requires that EEXIST be the
	 * errno return value -- I sure hope they're right.
	 */
	if (create_ok) {
		if ((ret = __os_open(dbenv, infop->name, 0,
		    DB_OSO_CREATE | DB_OSO_EXCL | DB_OSO_REGION,
		    dbenv->db_mode, &dbenv->lockfhp)) == 0)
			goto creation;
		if (ret != EEXIST) {
			__db_err(dbenv, ret, "%s", infop->name);
			goto err;
		}
	}

	/*
	 * If we couldn't create the file, try and open it.  (If that fails,
	 * we're done.)
	 */
	if ((ret = __os_open(
	    dbenv, infop->name, 0, DB_OSO_REGION, 0, &dbenv->lockfhp)) != 0)
		goto err;

	/* The region exists, it's not okay to recreate it. */
	F_CLR(infop, REGION_CREATE_OK);

	/*
	 * !!!
	 * The region may be in system memory not backed by the filesystem
	 * (more specifically, not backed by this file), and we're joining
	 * it.  In that case, the process that created it will have written
	 * out a REGENV_REF structure as its only contents.  We read that
	 * structure before we do anything further, e.g., we can't just map
	 * that file in and then figure out what's going on.
	 *
	 * All of this noise is because some systems don't have a coherent VM
	 * and buffer cache, and what's worse, when you mix operations on the
	 * VM and buffer cache, half the time you hang the system.
	 *
	 * If the file is the size of an REGENV_REF structure, then we know
	 * the real region is in some other memory.  (The only way you get a
	 * file that size is to deliberately write it, as it's smaller than
	 * any possible disk sector created by writing a file or mapping the
	 * file into memory.)  In which case, retrieve the structure from the
	 * file and use it to acquire the referenced memory.
	 *
	 * If the structure is larger than a REGENV_REF structure, then this
	 * file is backing the shared memory region, and we just map it into
	 * memory.
	 *
	 * And yes, this makes me want to take somebody and kill them.  (I
	 * digress -- but you have no freakin' idea.  This is unbelievably
	 * stupid and gross, and I've probably spent six months of my life,
	 * now, trying to make different versions of it work.)
	 */
	if ((ret = __os_ioinfo(dbenv, infop->name,
	    dbenv->lockfhp, &mbytes, &bytes, NULL)) != 0) {
		__db_err(dbenv, ret, "%s", infop->name);
		goto err;
	}

	/*
	 * !!!
	 * A size_t is OK -- regions get mapped into memory, and so can't
	 * be larger than a size_t.
	 */
	size = mbytes * MEGABYTE + bytes;

	/*
	 * If the size is less than the size of a REGENV_REF structure, the
	 * region (or, possibly, the REGENV_REF structure) has not yet been
	 * completely written.  Shouldn't be possible, but there's no reason
	 * not to wait awhile and try again.
	 *
	 * Otherwise, if the size is the size of a REGENV_REF structure,
	 * read it into memory and use it as a reference to the real region.
	 */
	if (size <= sizeof(ref)) {
		if (size != sizeof(ref))
			goto retry;

		if ((ret = __os_read(dbenv, dbenv->lockfhp, &ref,
		    sizeof(ref), &nrw)) != 0 || nrw < (size_t)sizeof(ref)) {
			if (ret == 0)
				ret = EIO;
			__db_err(dbenv, ret,
		    "%s: unable to read system-memory information",
			    infop->name);
			goto err;
		}
		size = ref.size;
		segid = ref.segid;

		F_SET(dbenv, DB_ENV_SYSTEM_MEM);
	} else if (F_ISSET(dbenv, DB_ENV_SYSTEM_MEM)) {
		ret = EINVAL;
		__db_err(dbenv, ret,
		    "%s: existing environment not created in system memory",
		    infop->name);
		goto err;
	} else
		segid = INVALID_REGION_SEGID;

#ifndef HAVE_MUTEX_FCNTL
	/*
	 * If we're not doing fcntl locking, we can close the file handle.  We
	 * no longer need it and the less contact between the buffer cache and
	 * the VM, the better.
	 */
	 (void)__os_closehandle(dbenv, dbenv->lockfhp);
	 dbenv->lockfhp = NULL;
#endif

	/* Call the region join routine to acquire the region. */
	memset(&tregion, 0, sizeof(tregion));
	tregion.size = (roff_t)size;
	tregion.segid = segid;
	if ((ret = __os_r_attach(dbenv, infop, &tregion)) != 0)
		goto err;

	/*
	 * The environment's REGENV structure has to live at offset 0 instead
	 * of the usual alloc information.  Set the primary reference and
	 * correct the "addr" value to reference the alloc region.  Note,
	 * this means that all of our offsets (R_ADDR/R_OFFSET) get shifted
	 * as well, but that should be fine.
	 */
	infop->primary = infop->addr;
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);
	renv = infop->primary;

	/*
	 * Make sure the region matches our build.  Special case a region
	 * that's all nul bytes, just treat it like any other corruption.
	 */
	if (renv->majver != DB_VERSION_MAJOR ||
	    renv->minver != DB_VERSION_MINOR) {
		if (renv->majver != 0 || renv->minver != 0) {
			__db_errx(dbenv,
	"Program version %d.%d doesn't match environment version %d.%d",
			    DB_VERSION_MAJOR, DB_VERSION_MINOR,
			    renv->majver, renv->minver);
			ret = DB_VERSION_MISMATCH;
		} else
			ret = EINVAL;
		goto err;
	}

	/*
	 * Check if the environment has had a catastrophic failure.
	 *
	 * Check the magic number to ensure the region is initialized.  If the
	 * magic number isn't set, the lock may not have been initialized, and
	 * an attempt to use it could lead to random behavior.
	 *
	 * The panic and magic values aren't protected by any lock, so we never
	 * use them in any check that's more complex than set/not-set.
	 *
	 * !!!
	 * I'd rather play permissions games using the underlying file, but I
	 * can't because Windows/NT filesystems won't open files mode 0.
	 */
	if (renv->panic && !F_ISSET(dbenv, DB_ENV_NOPANIC)) {
		ret = __db_panic_msg(dbenv);
		goto err;
	}
	if (renv->magic != DB_REGION_MAGIC)
		goto retry;

	/*
	 * Get a reference to the underlying REGION information for this
	 * environment.
	 */
	if ((ret = __env_des_get(dbenv, infop, infop, &rp)) != 0 || rp == NULL)
		goto find_err;
	infop->rp = rp;

	/*
	 * There's still a possibility for inconsistent data.  When we acquired
	 * the size of the region and attached to it, it might have still been
	 * growing as part of its creation.  We can detect this by checking the
	 * size we originally found against the region's current size.  (The
	 * region's current size has to be final, the creator finished growing
	 * it before setting the magic number in the region.)
	 */
	if (rp->size != size)
		goto retry;

	/*
	 * Check our callers configuration flags, it's an error to configure
	 * incompatible or additional subsystems in an existing environment.
	 * Return the total set of flags to the caller so they initialize the
	 * correct set of subsystems.
	 */
	if (init_flagsp != NULL) {
		FLD_CLR(*init_flagsp, renv->init_flags);
		if (*init_flagsp != 0) {
			__db_errx(dbenv,
    "configured environment flags incompatible with existing environment");
			ret = EINVAL;
			goto err;
		}
		*init_flagsp = renv->init_flags;
	}

	/*
	 * Fault the pages into memory.  Note, do this AFTER releasing the
	 * lock, because we're only reading the pages, not writing them.
	 */
	(void)__env_faultmem(dbenv, infop->primary, rp->size, 0);

	/* Everything looks good, we're done. */
	dbenv->reginfo = infop;
	return (0);

creation:
	/* Create the environment region. */
	F_SET(infop, REGION_CREATE);

	/*
	 * Allocate room for REGION structures plus overhead.
	 *
	 * XXX
	 * Overhead is so high because encryption passwds, replication vote
	 * arrays and the thread control block table are all stored in the
	 * base environment region.  This is a bug, at the least replication
	 * should have its own region.
	 *
	 * Allocate space for thread info blocks.  Max is only advisory,
	 * so we allocate 25% more.
	 */
	memset(&tregion, 0, sizeof(tregion));
	nregions = __memp_max_regions(dbenv) + 10;
	s = nregions * sizeof(REGION);
	s += dbenv->passwd_len;
	s += (dbenv->thr_max + dbenv->thr_max / 4) *
	    __env_alloc_size(sizeof(DB_THREAD_INFO));
	s += dbenv->thr_nbucket * __env_alloc_size(sizeof(DB_HASHTAB));
	s += 16 * 1024;
	tregion.size = s;
	tregion.segid = INVALID_REGION_SEGID;
	if ((ret = __os_r_attach(dbenv, infop, &tregion)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything, because we're writing the pages, not just reading them.
	 */
	(void)__env_faultmem(dbenv, infop->addr, tregion.size, 1);

	/*
	 * The first object in the region is the REGENV structure.  This is
	 * different from the other regions, and, from everything else in
	 * this region, where all objects are allocated from the pool, i.e.,
	 * there aren't any fixed locations.  The remaining space is made
	 * available for later allocation.
	 *
	 * The allocation space must be size_t aligned, because that's what
	 * the initialization routine is going to store there.  To make sure
	 * that happens, the REGENV structure was padded with a final size_t.
	 * No other region needs to worry about it because all of them treat
	 * the entire region as allocation space.
	 *
	 * Set the primary reference and correct the "addr" value to reference
	 * the alloc region.  Note, this requires that we "uncorrect" it at
	 * region detach, and that all of our offsets (R_ADDR/R_OFFSET) will be
	 * shifted as well, but that should be fine.
	 */
	infop->primary = infop->addr;
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);
	__env_alloc_init(infop, tregion.size - sizeof(REGENV));

	/*
	 * Initialize the rest of the REGENV structure.  (Don't set the magic
	 * number to the correct value, that would validate the environment).
	 */
	renv = infop->primary;
	renv->magic = 0;
	renv->panic = 0;

	(void)db_version(&majver, &minver, &patchver);
	renv->majver = (u_int32_t)majver;
	renv->minver = (u_int32_t)minver;
	renv->patchver = (u_int32_t)patchver;

	(void)time(&renv->timestamp);
	__os_unique_id(dbenv, &renv->envid);

	/*
	 * Initialize init_flags to store the flags that any other environment
	 * handle that uses DB_JOINENV to join this environment will need.
	 */
	renv->init_flags = (init_flagsp == NULL) ? 0 : *init_flagsp;

	/*
	 * Set up the region array.  We use an array rather than a linked list
	 * as we have to traverse this list after failure in some cases, and
	 * we don't want to infinitely loop should the application fail while
	 * we're manipulating the list.
	 */
	renv->region_cnt = nregions;
	if ((ret = __env_alloc(infop, nregions * sizeof(REGION), &rp)) != 0) {
		__db_err(
		    dbenv, ret, "unable to create new master region array");
		goto err;
	}
	renv->region_off = R_OFFSET(infop, rp);
	for (i = 0; i < nregions; ++i, ++rp)
		rp->id = INVALID_REGION_ID;

	renv->cipher_off = renv->thread_off = renv->rep_off = INVALID_ROFF;
	renv->flags = 0;
	renv->op_timestamp = renv->rep_timestamp = 0;
	renv->mtx_regenv = MUTEX_INVALID;

	/*
	 * Get the underlying REGION structure for this environment.  Note,
	 * we created the underlying OS region before we acquired the REGION
	 * structure, which is backwards from the normal procedure.  Update
	 * the REGION structure.
	 */
	if ((ret = __env_des_get(dbenv, infop, infop, &rp)) != 0) {
find_err:	__db_errx(dbenv, "%s: unable to find environment", infop->name);
		if (ret == 0)
			ret = EINVAL;
		goto err;
	}
	infop->rp = rp;
	rp->size = tregion.size;
	rp->segid = tregion.segid;

	/*
	 * !!!
	 * If we create an environment where regions are public and in system
	 * memory, we have to inform processes joining the environment how to
	 * attach to the shared memory segment.  So, we write the shared memory
	 * identifier into the file, to be read by those other processes.
	 *
	 * XXX
	 * This is really OS-layer information, but I can't see any easy way
	 * to move it down there without passing down information that it has
	 * no right to know, e.g., that this is the one-and-only REGENV region
	 * and not some other random region.
	 */
	if (tregion.segid != INVALID_REGION_SEGID) {
		ref.size = tregion.size;
		ref.segid = tregion.segid;
		if ((ret = __os_write(
		    dbenv, dbenv->lockfhp, &ref, sizeof(ref), &nrw)) != 0) {
			__db_err(dbenv, ret,
			    "%s: unable to write out public environment ID",
			    infop->name);
			goto err;
		}
	}

#ifndef HAVE_MUTEX_FCNTL
	/*
	 * If we're not doing fcntl locking, we can close the file handle.  We
	 * no longer need it and the less contact between the buffer cache and
	 * the VM, the better.
	 */
	if (dbenv->lockfhp != NULL) {
		 (void)__os_closehandle(dbenv, dbenv->lockfhp);
		 dbenv->lockfhp = NULL;
	}
#endif

	/* Everything looks good, we're done. */
	dbenv->reginfo = infop;
	return (0);

err:
retry:	/* Close any open file handle. */
	if (dbenv->lockfhp != NULL) {
		(void)__os_closehandle(dbenv, dbenv->lockfhp);
		dbenv->lockfhp = NULL;
	}

	/*
	 * If we joined or created the region, detach from it.  If we created
	 * it, destroy it.  Note, there's a path in the above code where we're
	 * using a temporary REGION structure because we haven't yet allocated
	 * the real one.  In that case the region address (addr) will be filled
	 * in, but the REGION pointer (rp) won't.  Fix it.
	 */
	if (infop->addr != NULL) {
		if (infop->rp == NULL)
			infop->rp = &tregion;

		/* Reset the addr value that we "corrected" above. */
		infop->addr = infop->primary;
		(void)__os_r_detach(dbenv,
		    infop, F_ISSET(infop, REGION_CREATE));
	}

	/* Free the allocated name and/or REGINFO structure. */
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);
	__os_free(dbenv, infop);

	/* If we had a temporary error, wait awhile and try again. */
	if (ret == 0) {
		if (!retry_ok || ++retry_cnt > 3) {
			__db_errx(dbenv, "unable to join the environment");
			ret = EAGAIN;
		} else {
			__os_sleep(dbenv, retry_cnt * 3, 0);
			goto loop;
		}
	}

	return (ret);
}

/*
 * __env_turn_on --
 *	Turn on the created environment.
 *
 * PUBLIC: int __env_turn_on __P((DB_ENV *));
 */
int
__env_turn_on(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;

	infop = dbenv->reginfo;
	renv = infop->primary;

	/* If we didn't create the region, there's no need for further work. */
	if (!F_ISSET(infop, REGION_CREATE))
		return (0);

	/*
	 * Validate the file.  All other threads of control are waiting
	 * on this value to be written -- "Let slip the hounds of war!"
	 */
	renv->magic = DB_REGION_MAGIC;

	return (0);
}

/*
 * __env_turn_off --
 *	Turn off the environment.
 *
 * PUBLIC: int __env_turn_off __P((DB_ENV *, u_int32_t));
 */
int
__env_turn_off(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	REGENV *renv;
	REGINFO *infop;
	int ret, t_ret;

	ret = 0;

	/*
	 * Connect to the environment: If we can't join the environment, we
	 * guess it's because it doesn't exist and we're done.
	 *
	 * If the environment exists, attach and lock the environment.
	 */
	if (__env_attach(dbenv, NULL, 0, 1) != 0)
		return (0);

	infop = dbenv->reginfo;
	renv = infop->primary;

	MUTEX_LOCK(dbenv, renv->mtx_regenv);

	/*
	 * If the environment is in use, we're done unless we're forcing the
	 * issue or the environment has panic'd.  (If the environment panic'd,
	 * the thread holding the reference count may not have cleaned up, so
	 * we clean up.  It's possible the application didn't plan on removing
	 * the environment in this particular call, but panic'd environments
	 * aren't useful to anyone.)
	 *
	 * Otherwise, panic the environment and overwrite the magic number so
	 * any thread of control attempting to connect (or racing with us) will
	 * back off and retry, or just die.
	 */
	if (renv->refcnt > 0 && !LF_ISSET(DB_FORCE) && !renv->panic)
		ret = EBUSY;
	else
		renv->panic = 1;

	/*
	 * Unlock the environment (nobody should need this lock because
	 * we've poisoned the pool) and detach from the environment.
	 */
	MUTEX_UNLOCK(dbenv, renv->mtx_regenv);

	if ((t_ret = __env_detach(dbenv, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __env_panic_set --
 *	Set/clear unrecoverable error.
 *
 * PUBLIC: void __env_panic_set __P((DB_ENV *, int));
 */
void
__env_panic_set(dbenv, on)
	DB_ENV *dbenv;
	int on;
{
	if (dbenv != NULL && dbenv->reginfo != NULL)
		((REGENV *)
		    ((REGINFO *)dbenv->reginfo)->primary)->panic = on ? 1 : 0;
}

/*
 * __env_ref_increment --
 *	Increment the environment's reference count.
 *
 * PUBLIC: int __env_ref_increment __P((DB_ENV *));
 */
int
__env_ref_increment(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;
	int ret;

	infop = dbenv->reginfo;
	renv = infop->primary;

	/* If we're creating the primary region, allocate a mutex. */
	if (F_ISSET(infop, REGION_CREATE)) {
		if ((ret = __mutex_alloc(
		    dbenv, MTX_ENV_REGION, 0, &renv->mtx_regenv)) != 0)
			return (ret);
		renv->refcnt = 1;
	} else {
		/* Lock the environment, increment the reference, unlock. */
		MUTEX_LOCK(dbenv, renv->mtx_regenv);
		++renv->refcnt;
		MUTEX_UNLOCK(dbenv, renv->mtx_regenv);
	}

	F_SET(dbenv, DB_ENV_REF_COUNTED);
	return (0);
}

/*
 * __env_ref_decrement --
 *	Decrement the environment's reference count.
 *
 * PUBLIC: int __env_ref_decrement __P((DB_ENV *));
 */
int
__env_ref_decrement(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop;

	/* Be cautious -- we may not have an environment. */
	if ((infop = dbenv->reginfo) == NULL)
		return (0);

	renv = infop->primary;

	/* Even if we have an environment, may not have reference counted it. */
	if (F_ISSET(dbenv, DB_ENV_REF_COUNTED)) {
		/* Lock the environment, decrement the reference, unlock. */
		MUTEX_LOCK(dbenv, renv->mtx_regenv);
		if (renv->refcnt == 0)
			__db_errx(dbenv,
			    "environment reference count went negative");
		else
			--renv->refcnt;
		MUTEX_UNLOCK(dbenv, renv->mtx_regenv);

		F_CLR(dbenv, DB_ENV_REF_COUNTED);
	}

	/* If a private environment, we're done with the mutex, destroy it. */
	return (F_ISSET(dbenv, DB_ENV_PRIVATE) ?
	    __mutex_free(dbenv, &renv->mtx_regenv) : 0);
}

/*
 * __env_detach --
 *	Detach from the environment.
 *
 * PUBLIC: int __env_detach __P((DB_ENV *, int));
 */
int
__env_detach(dbenv, destroy)
	DB_ENV *dbenv;
	int destroy;
{
	REGENV *renv;
	REGINFO *infop;
	REGION rp;
	int ret, t_ret;

	infop = dbenv->reginfo;
	renv = infop->primary;
	ret = 0;

	/* Close the locking file handle. */
	if (dbenv->lockfhp != NULL) {
		if ((t_ret =
		    __os_closehandle(dbenv, dbenv->lockfhp)) != 0 && ret == 0)
			ret = t_ret;
		dbenv->lockfhp = NULL;
	}

	/*
	 * If a private region, return the memory to the heap.  Not needed for
	 * filesystem-backed or system shared memory regions, that memory isn't
	 * owned by any particular process.
	 */
	if (destroy) {
		/*
		 * Free the REGION array.
		 *
		 * The actual underlying region structure is allocated from the
		 * primary shared region, and we're about to free it.  Save a
		 * copy on our stack for the REGINFO to reference when it calls
		 * down into the OS layer to release the shared memory segment.
		 */
		rp = *infop->rp;
		infop->rp = &rp;

		if (renv->region_off != INVALID_ROFF)
			__env_alloc_free(
			   infop, R_ADDR(infop, renv->region_off));
	}

	/*
	 * Set the DB_ENV->reginfo field to NULL.  BDB uses the DB_ENV->reginfo
	 * field to decide if the underlying region can be accessed or needs
	 * cleanup.  We're about to destroy what it references, so it needs to
	 * be cleared.
	 */
	dbenv->reginfo = NULL;

	/* Reset the addr value that we "corrected" above. */
	infop->addr = infop->primary;

	if ((t_ret = __os_r_detach(dbenv, infop, destroy)) != 0 && ret == 0)
		ret = t_ret;
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);

	/* Discard the DB_ENV->reginfo field's memory. */
	__os_free(dbenv, infop);

	return (ret);
}

/*
 * __env_remove_env --
 *	Remove an environment.
 *
 * PUBLIC: int __env_remove_env __P((DB_ENV *));
 */
int
__env_remove_env(dbenv)
	DB_ENV *dbenv;
{
	REGENV *renv;
	REGINFO *infop, reginfo;
	REGION *rp;
	u_int32_t flags_orig, i;

	/*
	 * We do not want to hang on a mutex request, nor do we care about
	 * panics.
	 */
	flags_orig = F_ISSET(dbenv, DB_ENV_NOLOCKING | DB_ENV_NOPANIC);
	F_SET(dbenv, DB_ENV_NOLOCKING | DB_ENV_NOPANIC);

	/*
	 * This routine has to walk a nasty line between not looking into the
	 * environment (which may be corrupted after an app or system crash),
	 * and removing everything that needs removing.
	 *
	 * Connect to the environment: If we can't join the environment, we
	 * guess it's because it doesn't exist.  Remove the underlying files,
	 * at least.
	 */
	if (__env_attach(dbenv, NULL, 0, 0) != 0)
		goto remfiles;

	infop = dbenv->reginfo;
	renv = infop->primary;

	/*
	 * Kill the environment, if it's not already dead.
	 */
	renv->panic = 1;

	/*
	 * Walk the array of regions.  Connect to each region and disconnect
	 * with the destroy flag set.  This shouldn't cause any problems, even
	 * if the region is corrupted, because we never look inside the region
	 * (with the single exception of mutex regions on systems where we have
	 * to return resources to the underlying system).
	 */
	for (rp = R_ADDR(infop, renv->region_off),
	    i = 0; i < renv->region_cnt; ++i, ++rp) {
		if (rp->id == INVALID_REGION_ID || rp->type == REGION_TYPE_ENV)
			continue;
		/*
		 * !!!
		 * The REGION_CREATE_OK flag is set for Windows/95 -- regions
		 * are zero'd out when the last reference to the region goes
		 * away, in which case the underlying OS region code requires
		 * callers be prepared to create the region in order to join it.
		 */
		memset(&reginfo, 0, sizeof(reginfo));
		reginfo.id = rp->id;
		reginfo.flags = REGION_CREATE_OK;

		/*
		 * If we get here and can't attach and/or detach to the
		 * region, it's a mess.  Ignore errors, there's nothing
		 * we can do about them.
		 */
		if (__env_region_attach(dbenv, &reginfo, 0) != 0)
			continue;

#ifdef  HAVE_MUTEX_SYSTEM_RESOURCES
		/*
		 * If destroying the mutex region, return any system
		 * resources to the system.
		 */
		if (reginfo.type == REGION_TYPE_MUTEX)
			__mutex_resource_return(dbenv, &reginfo);
#endif
		(void)__env_region_detach(dbenv, &reginfo, 1);
	}

	/* Detach from the environment's primary region. */
	(void)__env_detach(dbenv, 1);

remfiles:
	/*
	 * Walk the list of files in the directory, unlinking files in the
	 * Berkeley DB name space.
	 */
	__env_remove_file(dbenv);

	F_CLR(dbenv, DB_ENV_NOLOCKING | DB_ENV_NOPANIC);
	F_SET(dbenv, flags_orig);

	return (0);
}

/*
 * __env_remove_file --
 *	Discard any region files in the filesystem.
 */
static void
__env_remove_file(dbenv)
	DB_ENV *dbenv;
{
	int cnt, fcnt, lastrm, ret;
	const char *dir;
	char saved_char, *p, **names, *path, buf[sizeof(DB_REGION_FMT) + 20];

	/* Get the full path of a file in the environment. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret = __db_appname(dbenv, DB_APP_NONE, buf, 0, NULL, &path)) != 0)
		return;

	/* Get the parent directory for the environment. */
	if ((p = __db_rpath(path)) == NULL) {
		p = path;
		saved_char = *p;

		dir = PATH_DOT;
	} else {
		saved_char = *p;
		*p = '\0';

		dir = path;
	}

	/* Get the list of file names. */
	if ((ret = __os_dirlist(dbenv, dir, &names, &fcnt)) != 0)
		__db_err(dbenv, ret, "%s", dir);

	/* Restore the path, and free it. */
	*p = saved_char;
	__os_free(dbenv, path);

	if (ret != 0)
		return;

	/*
	 * Remove files from the region directory.
	 */
	for (lastrm = -1, cnt = fcnt; --cnt >= 0;) {
		/* Skip anything outside our name space. */
		if (strncmp(names[cnt],
		    DB_REGION_PREFIX, sizeof(DB_REGION_PREFIX) - 1))
			continue;

		/* Skip queue extent files. */
		if (strncmp(names[cnt], "__dbq.", 6) == 0)
			continue;

		/* Skip registry files. */
		if (strncmp(names[cnt], "__db.register", 13) == 0)
			continue;

		/* Skip replication files. */
		if (strncmp(names[cnt], "__db.rep", 8) == 0)
			continue;

		/*
		 * Remove the primary environment region last, because it's
		 * the key to this whole mess.
		 */
		if (strcmp(names[cnt], DB_REGION_ENV) == 0) {
			lastrm = cnt;
			continue;
		}

		/* Remove the file. */
		if (__db_appname(dbenv,
		    DB_APP_NONE, names[cnt], 0, NULL, &path) == 0) {
			/*
			 * Overwrite region files.  Temporary files would have
			 * been maintained in encrypted format, so there's no
			 * reason to overwrite them.  This is not an exact
			 * check on the file being a region file, but it's
			 * not likely to be wrong, and the worst thing that can
			 * happen is we overwrite a file that didn't need to be
			 * overwritten.
			 */
			if (F_ISSET(dbenv, DB_ENV_OVERWRITE) &&
			    strlen(names[cnt]) == DB_REGION_NAME_LENGTH)
				(void)__db_file_multi_write(dbenv, path);
			(void)__os_unlink(dbenv, path);
			__os_free(dbenv, path);
		}
	}

	if (lastrm != -1)
		if (__db_appname(dbenv,
		    DB_APP_NONE, names[lastrm], 0, NULL, &path) == 0) {
			if (F_ISSET(dbenv, DB_ENV_OVERWRITE))
				(void)__db_file_multi_write(dbenv, path);
			(void)__os_unlink(dbenv, path);
			__os_free(dbenv, path);
		}
	__os_dirfree(dbenv, names, fcnt);
}

/*
 * __env_region_attach
 *	Join/create a region.
 *
 * PUBLIC: int __env_region_attach __P((DB_ENV *, REGINFO *, size_t));
 */
int
__env_region_attach(dbenv, infop, size)
	DB_ENV *dbenv;
	REGINFO *infop;
	size_t size;
{
	REGION *rp;
	int ret;
	char buf[sizeof(DB_REGION_FMT) + 20];

	/*
	 * Find or create a REGION structure for this region.  If we create
	 * it, the REGION_CREATE flag will be set in the infop structure.
	 */
	F_CLR(infop, REGION_CREATE);
	if ((ret = __env_des_get(dbenv, dbenv->reginfo, infop, &rp)) != 0)
		return (ret);
	infop->dbenv = dbenv;
	infop->rp = rp;
	infop->type = rp->type;
	infop->id = rp->id;

	/*
	 * __env_des_get may have created the region and reset the create
	 * flag.  If we're creating the region, set the desired size.
	 */
	if (F_ISSET(infop, REGION_CREATE))
		rp->size = (roff_t)size;

	/* Join/create the underlying region. */
	(void)snprintf(buf, sizeof(buf), DB_REGION_FMT, infop->id);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, buf, 0, NULL, &infop->name)) != 0)
		goto err;
	if ((ret = __os_r_attach(dbenv, infop, rp)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything because we're writing pages in created regions, not just
	 * reading them.
	 */
	(void)__env_faultmem(dbenv,
	    infop->addr, rp->size, F_ISSET(infop, REGION_CREATE));

	/*
	 * !!!
	 * The underlying layer may have just decided that we are going
	 * to create the region.  There are various system issues that
	 * can result in a useless region that requires re-initialization.
	 *
	 * If we created the region, initialize it for allocation.
	 */
	if (F_ISSET(infop, REGION_CREATE))
		__env_alloc_init(infop, rp->size);

	return (0);

err:	/* Discard the underlying region. */
	if (infop->addr != NULL)
		(void)__os_r_detach(dbenv,
		    infop, F_ISSET(infop, REGION_CREATE));
	infop->rp = NULL;
	infop->id = INVALID_REGION_ID;

	/* Discard the REGION structure if we created it. */
	if (F_ISSET(infop, REGION_CREATE)) {
		__env_des_destroy(dbenv, rp);
		F_CLR(infop, REGION_CREATE);
	}

	return (ret);
}

/*
 * __env_region_detach --
 *	Detach from a region.
 *
 * PUBLIC: int __env_region_detach __P((DB_ENV *, REGINFO *, int));
 */
int
__env_region_detach(dbenv, infop, destroy)
	DB_ENV *dbenv;
	REGINFO *infop;
	int destroy;
{
	REGION *rp;
	int ret;

	rp = infop->rp;
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		destroy = 1;

	/*
	 * When discarding the regions as we shut down a database environment,
	 * discard any allocated shared memory segments.  This is the last time
	 * we use them, and db_region_destroy is the last region-specific call
	 * we make.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE) && infop->primary != NULL)
		__env_alloc_free(infop, infop->primary);

	/* Detach from the underlying OS region. */
	ret = __os_r_detach(dbenv, infop, destroy);

	/* If we destroyed the region, discard the REGION structure. */
	if (destroy)
		__env_des_destroy(dbenv, rp);

	/* Destroy the structure. */
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);

	return (ret);
}

/*
 * __env_des_get --
 *	Return a reference to the shared information for a REGION,
 *	optionally creating a new entry.
 */
static int
__env_des_get(dbenv, env_infop, infop, rpp)
	DB_ENV *dbenv;
	REGINFO *env_infop, *infop;
	REGION **rpp;
{
	REGENV *renv;
	REGION *rp, *empty_slot, *first_type;
	u_int32_t i, maxid;

	*rpp = NULL;
	renv = env_infop->primary;

	/*
	 * If the caller wants to join a region, walk through the existing
	 * regions looking for a matching ID (if ID specified) or matching
	 * type (if type specified).  If we return based on a matching type
	 * return the "primary" region, that is, the first region that was
	 * created of this type.
	 *
	 * Track the first empty slot and maximum region ID for new region
	 * allocation.
	 *
	 * MaxID starts at REGION_ID_ENV, the ID of the primary environment.
	 */
	maxid = REGION_ID_ENV;
	empty_slot = first_type = NULL;
	for (rp = R_ADDR(env_infop, renv->region_off),
	    i = 0; i < renv->region_cnt; ++i, ++rp) {
		if (rp->id == INVALID_REGION_ID) {
			if (empty_slot == NULL)
				empty_slot = rp;
			continue;
		}
		if (infop->id != INVALID_REGION_ID) {
			if (infop->id == rp->id)
				break;
			continue;
		}
		if (infop->type == rp->type &&
		    F_ISSET(infop, REGION_JOIN_OK) &&
		    (first_type == NULL || first_type->id > rp->id))
			first_type = rp;

		if (rp->id > maxid)
			maxid = rp->id;
	}

	/* If we found a matching ID (or a matching type), return it. */
	if (i >= renv->region_cnt)
		rp = first_type;
	if (rp != NULL) {
		*rpp = rp;
		return (0);
	}

	/*
	 * If we didn't find a region and we don't have permission to create
	 * the region, fail.  The caller generates any error message.
	 */
	if (!F_ISSET(infop, REGION_CREATE_OK))
		return (ENOENT);

	/*
	 * If we didn't find a region and don't have room to create the region
	 * fail with an error message, there's a sizing problem.
	 */
	if (empty_slot == NULL) {
		__db_errx(dbenv, "no room remaining for additional REGIONs");
		return (ENOENT);
	}

	/*
	 * Initialize a REGION structure for the caller.  If id was set, use
	 * that value, otherwise we use the next available ID.
	 */
	memset(empty_slot, 0, sizeof(REGION));
	empty_slot->segid = INVALID_REGION_SEGID;

	/*
	 * Set the type and ID; if no region ID was specified,
	 * allocate one.
	 */
	empty_slot->type = infop->type;
	empty_slot->id = infop->id == INVALID_REGION_ID ? maxid + 1 : infop->id;

	F_SET(infop, REGION_CREATE);

	*rpp = empty_slot;
	return (0);
}

/*
 * __env_des_destroy --
 *	Destroy a reference to a REGION.
 */
static void
__env_des_destroy(dbenv, rp)
	DB_ENV *dbenv;
	REGION *rp;
{
	COMPQUIET(dbenv, NULL);

	rp->id = INVALID_REGION_ID;
}

/*
 * __env_faultmem --
 *	Fault the region into memory.
 */
static int
__env_faultmem(dbenv, addr, size, created)
	DB_ENV *dbenv;
	void *addr;
	size_t size;
	int created;
{
	int ret;
	u_int8_t *p, *t;

	/* Ignore heap regions. */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		return (0);

	/*
	 * It's sometimes significantly faster to page-fault in all of the
	 * region's pages before we run the application, as we see nasty
	 * side-effects when we page-fault while holding various locks, i.e.,
	 * the lock takes a long time to acquire because of the underlying
	 * page fault, and the other threads convoy behind the lock holder.
	 *
	 * If we created the region, we write a non-zero value so that the
	 * system can't cheat.  If we're just joining the region, we can
	 * only read the value and try to confuse the compiler sufficiently
	 * that it doesn't figure out that we're never really using it.
	 *
	 * Touch every page (assuming pages are 512B, the smallest VM page
	 * size used in any general purpose processor).
	 */
	ret = 0;
	if (F_ISSET(dbenv, DB_ENV_REGION_INIT)) {
		if (created)
			for (p = addr,
			    t = (u_int8_t *)addr + size; p < t; p += 512)
				p[0] = 0xdb;
		else
			for (p = addr,
			    t = (u_int8_t *)addr + size; p < t; p += 512)
				ret |= p[0];
	}

	return (ret);
}
