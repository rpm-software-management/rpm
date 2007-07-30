/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id: env_alloc.c,v 12.17 2007/05/22 18:35:39 ubell Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * Implement shared memory region allocation.  The initial list is a single
 * memory "chunk" which is carved up as memory is requested.  Chunks are
 * coalesced when free'd.  We maintain two linked-lists of chunks: a list of
 * all chunks sorted by address, and a list of free chunks sorted by size.
 *
 * The ALLOC_LAYOUT structure is the governing structure for the chunk.
 *
 * The ALLOC_ELEMENT structure is the structure that describes any single
 * chunk of memory, and is immediately followed by the user's memory.
 *
 * The internal memory chunks are always aligned to a uintmax_t boundary so
 * we don't drop core accessing the fields of the ALLOC_ELEMENT structure.
 *
 * The memory chunks returned to the user are aligned to a uintmax_t boundary.
 * This is enforced by terminating the ALLOC_ELEMENT structure with a uintmax_t
 * field as that immediately precedes the user's memory.
 *
 * Any caller needing more than uintmax_t alignment is responsible for doing
 * the work themselves.
 */
typedef struct __alloc_layout {
	SH_TAILQ_HEAD(__addrq) addrq;		/* Sorted by address */
	SH_TAILQ_HEAD(__sizeq) sizeq;		/* Sorted by size */

#ifdef HAVE_STATISTICS
	u_int32_t success;			/* Successful allocations */
	u_int32_t failure;			/* Failed allocations */
	u_int32_t freed;			/* Free calls */
	u_int32_t longest;			/* Longest chain walked */
#ifdef __ALLOC_DISPLAY_ALLOCATION_SIZES
	u_int32_t pow2_size[32];		/* Allocation size tracking */
#endif
#endif
	uintmax_t  unused;			/* Guarantee alignment */
} ALLOC_LAYOUT;

typedef struct __alloc_element {
	SH_TAILQ_ENTRY addrq;			/* List by address */
	SH_TAILQ_ENTRY sizeq;			/* List by size */

	/*
	 * The "len" field is the total length of the chunk, not the size
	 * available to the caller.
	 */
	size_t len;				/* Chunk length */

	/*
	 * The "ulen" field is the length returned to the caller.
	 *
	 * Set to 0 if the chunk is not currently in use.
	 */
	uintmax_t ulen;				/* User's length */
} ALLOC_ELEMENT;

/*
 * __env_alloc_init --
 *	Initialize the area as one large chunk.
 *
 * PUBLIC: void __env_alloc_init __P((REGINFO *, size_t));
 */
void
__env_alloc_init(infop, size)
	REGINFO *infop;
	size_t size;
{
	DB_ENV *dbenv;
	ALLOC_ELEMENT *elp;
	ALLOC_LAYOUT *head;

	dbenv = infop->dbenv;

	/* No initialization needed for heap memory regions. */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		return;

	/*
	 * The first chunk of memory is the ALLOC_LAYOUT structure.
	 */
	head = infop->addr;
	SH_TAILQ_INIT(&head->addrq);
	SH_TAILQ_INIT(&head->sizeq);
	STAT((head->success = head->failure = head->freed = 0));
	COMPQUIET(head->unused, 0);

	/*
	 * The rest of the memory is the first available chunk.
	 */
	elp = (ALLOC_ELEMENT *)((u_int8_t *)head + sizeof(ALLOC_LAYOUT));
	elp->len = size - sizeof(ALLOC_LAYOUT);
	elp->ulen = 0;

	SH_TAILQ_INSERT_HEAD(&head->addrq, elp, addrq, __alloc_element);
	SH_TAILQ_INSERT_HEAD(&head->sizeq, elp, sizeq, __alloc_element);
}

/*
 * The length, the ALLOC_ELEMENT structure and an optional guard byte,
 * rounded up to standard alignment.
 */
#ifdef DIAGNOSTIC
#define	DB_ALLOC_SIZE(len)						\
	(size_t)DB_ALIGN((len) + sizeof(ALLOC_ELEMENT) + 1, sizeof(uintmax_t))
#else
#define	DB_ALLOC_SIZE(len)						\
	(size_t)DB_ALIGN((len) + sizeof(ALLOC_ELEMENT), sizeof(uintmax_t))
#endif

/*
 * __env_alloc_overhead --
 *	Return the overhead needed for an allocation.
 *
 * PUBLIC: size_t __env_alloc_overhead __P((void));
 */
size_t
__env_alloc_overhead()
{
	return (sizeof(ALLOC_ELEMENT));
}

/*
 * __env_alloc_size --
 *	Return the space needed for an allocation, including alignment.
 *
 * PUBLIC: size_t __env_alloc_size __P((size_t));
 */
size_t
__env_alloc_size(len)
	size_t len;
{
	return (DB_ALLOC_SIZE(len));
}

/*
 * __env_alloc --
 *	Allocate space from the shared region.
 *
 * PUBLIC: int __env_alloc __P((REGINFO *, size_t, void *));
 */
int
__env_alloc(infop, len, retp)
	REGINFO *infop;
	size_t len;
	void *retp;
{
	ALLOC_ELEMENT *elp, *frag, *elp_tmp;
	ALLOC_LAYOUT *head;
	DB_ENV *dbenv;
	size_t total_len;
	u_int8_t *p;
	int ret;
#ifdef HAVE_STATISTICS
	u_long st_search;
#endif

	dbenv = infop->dbenv;

	*(void **)retp = NULL;

	/*
	 * In a heap-backed environment, we call malloc for additional space.
	 * (Malloc must return memory correctly aligned for our use.)
	 *
	 * In a heap-backed environment, memory is laid out as follows:
	 *
	 * { size_t total-length } { user-memory } { guard-byte }
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		/* Check if we're over the limit. */
		if (infop->allocated >= infop->max_alloc)
			return (ENOMEM);

		/* We need an additional size_t to hold the length. */
		len += sizeof(size_t);

#ifdef DIAGNOSTIC
		/* Plus one byte for the guard byte. */
		++len;
#endif
		/* Allocate the space. */
		if ((ret = __os_malloc(dbenv, len, &p)) != 0)
			return (ret);
		infop->allocated += len;

		*(size_t *)p = len;
#ifdef DIAGNOSTIC
		p[len - 1] = GUARD_BYTE;
#endif
		*(void **)retp = p + sizeof(size_t);
		return (0);
	}

	head = infop->addr;

	total_len = DB_ALLOC_SIZE(len);
#ifdef __ALLOC_DISPLAY_ALLOCATION_SIZES
	STAT((++head->pow2_size[__db_log2(len) - 1]));
#endif

	/*
	 * If the chunk can be split into two pieces, with the fragment holding
	 * at least 64 bytes of memory, we divide the chunk into two parts.
	 */
#define	SHALLOC_FRAGMENT	(sizeof(ALLOC_ELEMENT) + 64)

	/*
	 * Walk the size queue, looking for the smallest slot that satisfies
	 * the request.
	 */
	elp = NULL;
	STAT((st_search = 0));
	SH_TAILQ_FOREACH(elp_tmp, &head->sizeq, sizeq, __alloc_element) {
		STAT((++st_search));
		if (elp_tmp->len < total_len)
			break;
		elp = elp_tmp;
		/*
		 * We might have a long list of chunks of the same size.  Stop
		 * looking if we won't fragment memory by picking the current
		 * slot.
		 */
		if (elp->len - total_len <= SHALLOC_FRAGMENT)
			break;
	}
#ifdef HAVE_STATISTICS
	if (head->longest < st_search)
		head->longest = st_search;
#endif

	/*
	 * If we don't find an element of the right size, we're done.
	 */
	if (elp == NULL) {
		STAT((++head->failure));
		return (ENOMEM);
	}
	STAT((++head->success));

	/* Pull the chunk off of the size queue. */
	SH_TAILQ_REMOVE(&head->sizeq, elp, sizeq, __alloc_element);

	if (elp->len - total_len > SHALLOC_FRAGMENT) {
		frag = (ALLOC_ELEMENT *)((u_int8_t *)elp + total_len);
		frag->len = elp->len - total_len;
		frag->ulen = 0;

		elp->len = total_len;

		/* The fragment follows the chunk on the address queue. */
		SH_TAILQ_INSERT_AFTER(
		    &head->addrq, elp, frag, addrq, __alloc_element);

		/*
		 * Insert the fragment into the appropriate place in the
		 * size queue.
		 */
		SH_TAILQ_FOREACH(elp_tmp, &head->sizeq, sizeq, __alloc_element)
			if (elp_tmp->len < frag->len)
				break;
		if (elp_tmp == NULL)
			SH_TAILQ_INSERT_TAIL(&head->sizeq, frag, sizeq);
		else
			SH_TAILQ_INSERT_BEFORE(&head->sizeq,
			    elp_tmp, frag, sizeq, __alloc_element);
	}

	p = (u_int8_t *)elp + sizeof(ALLOC_ELEMENT);
	elp->ulen = len;
#ifdef DIAGNOSTIC
	p[len] = GUARD_BYTE;
#endif
	*(void **)retp = p;

	return (0);
}

/*
 * __env_alloc_free --
 *	Free space into the shared region.
 *
 * PUBLIC: void __env_alloc_free __P((REGINFO *, void *));
 */
void
__env_alloc_free(infop, ptr)
	REGINFO *infop;
	void *ptr;
{
	ALLOC_ELEMENT *elp, *elp_tmp;
	ALLOC_LAYOUT *head;
	DB_ENV *dbenv;
	size_t len;
	u_int8_t *p;

	dbenv = infop->dbenv;

	/* In a private region, we call free. */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		/* Find the start of the memory chunk and its length. */
		p = (u_int8_t *)((size_t *)ptr - 1);
		len = *(size_t *)p;

		infop->allocated -= len;

#ifdef DIAGNOSTIC
		/* Check the guard byte. */
		DB_ASSERT(dbenv, p[len - 1] == GUARD_BYTE);

		/* Trash the memory chunk. */
		memset(p, CLEAR_BYTE, len);
#endif
		__os_free(dbenv, p);
		return;
	}

	head = infop->addr;
	STAT((++head->freed));

	p = ptr;
	elp = (ALLOC_ELEMENT *)(p - sizeof(ALLOC_ELEMENT));

#ifdef DIAGNOSTIC
	/* Check the guard byte. */
	DB_ASSERT(dbenv, p[elp->ulen] == GUARD_BYTE);

	/* Trash the memory chunk. */
	memset(p, CLEAR_BYTE, elp->len - sizeof(ALLOC_ELEMENT));
#endif

	/* Mark the memory as no longer in use. */
	elp->ulen = 0;

	/*
	 * Try and merge this chunk with chunks on either side of it.  Two
	 * chunks can be merged if they're contiguous and not in use.
	 */
	if ((elp_tmp =
	    SH_TAILQ_PREV(&head->addrq, elp, addrq, __alloc_element)) != NULL &&
	    elp_tmp->ulen == 0 &&
	    (u_int8_t *)elp_tmp + elp_tmp->len == (u_int8_t *)elp) {
		/*
		 * If we're merging the entry into a previous entry, remove the
		 * current entry from the addr queue and the previous entry from
		 * the size queue, and merge.
		 */
		SH_TAILQ_REMOVE(&head->addrq, elp, addrq, __alloc_element);
		SH_TAILQ_REMOVE(&head->sizeq, elp_tmp, sizeq, __alloc_element);

		elp_tmp->len += elp->len;
		elp = elp_tmp;
	}
	if ((elp_tmp = SH_TAILQ_NEXT(elp, addrq, __alloc_element)) != NULL &&
	    elp_tmp->ulen == 0 &&
	    (u_int8_t *)elp + elp->len == (u_int8_t *)elp_tmp) {
		/*
		 * If we're merging the current entry into a subsequent entry,
		 * remove the subsequent entry from the addr and size queues
		 * and merge.
		 */
		SH_TAILQ_REMOVE(&head->addrq, elp_tmp, addrq, __alloc_element);
		SH_TAILQ_REMOVE(&head->sizeq, elp_tmp, sizeq, __alloc_element);

		elp->len += elp_tmp->len;
	}

	/* Find the correct slot in the size queue. */
	SH_TAILQ_FOREACH(elp_tmp, &head->sizeq, sizeq, __alloc_element)
		if (elp->len >= elp_tmp->len)
			break;
	if (elp_tmp == NULL)
		SH_TAILQ_INSERT_TAIL(&head->sizeq, elp, sizeq);
	else
		SH_TAILQ_INSERT_BEFORE(&head->sizeq,
		    elp_tmp, elp, sizeq, __alloc_element);
}

#ifdef HAVE_STATISTICS
/*
 * __env_alloc_print --
 *	Display the lists of memory chunks.
 *
 * PUBLIC: void __env_alloc_print __P((REGINFO *, u_int32_t));
 */
void
__env_alloc_print(infop, flags)
	REGINFO *infop;
	u_int32_t flags;
{
#ifdef __ALLOC_DISPLAY_ALLOCATION_LISTS
	ALLOC_ELEMENT *elp;
#endif
	ALLOC_LAYOUT *head;
	DB_ENV *dbenv;
#ifdef __ALLOC_DISPLAY_ALLOCATION_SIZES
	DB_MSGBUF mb;
	int i;
#endif

	dbenv = infop->dbenv;
	head = infop->addr;

	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		return;

	__db_msg(dbenv,
    "Region allocations: %lu allocations, %lu failures, %lu frees, %lu longest",
	    (u_long)head->success, (u_long)head->failure, (u_long)head->freed,
	    (u_long)head->longest);

	if (!LF_ISSET(DB_STAT_ALL))
		return;

#ifdef __ALLOC_DISPLAY_ALLOCATION_SIZES
	/*
	 * We don't normally display the allocation history by size, it's too
	 * expensive to calculate.
	 */
	DB_MSGBUF_INIT(&mb);
	for (i = 0; i < 32; ++i)
		__db_msgadd(dbenv, &mb, "%s%2d/%lu",
		    i == 0 ? "Allocations by power-of-two sizes: " : ", ",
		    i + 1, (u_long)head->pow2_size[i]);
	DB_MSGBUF_FLUSH(dbenv, &mb);
#endif

#ifdef __ALLOC_DISPLAY_ALLOCATION_LISTS
	/*
	 * We don't normally display the list of address/chunk pairs, a few
	 * thousand lines of output is too voluminous for even DB_STAT_ALL.
	 */
	__db_msg(dbenv,
	    "Allocation list by address: {chunk length, user length}");
	SH_TAILQ_FOREACH(elp, &head->addrq, addrq, __alloc_element)
		__db_msg(dbenv, "\t%#lx {%lu, %lu}",
		    P_TO_ULONG(elp), (u_long)elp->len, (u_long)elp->ulen);

	__db_msg(dbenv, "Allocation free list by size: {chunk length}");
	SH_TAILQ_FOREACH(elp, &head->sizeq, sizeq, __alloc_element)
		__db_msg(dbenv, "\t%#lx {%lu}",
		    P_TO_ULONG(elp), (u_long)elp->len);
#endif
	return;
}
#endif
