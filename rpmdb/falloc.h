#ifndef H_FALLOC
#define H_FALLOC

/** \ingroup db1
 * \file rpmdb/falloc.h
 * File space allocation routines.
 *
 * Best fit allocation is used, free blocks are compacted. Minimal
 * fragmentation is more important then speed. This uses 32 bit
 * offsets on all platforms and should be byte order independent.
 */

/*@access FD_t@*/

#ifdef __cplusplus
extern "C" {
#endif

/*@unused@*/ static inline long int fadGetFileSize(FD_t fd) /*@*/ {
    return fd->fileSize;
}

/*@unused@*/ static inline void fadSetFileSize(FD_t fd, long int fileSize)
	/*@modifies fd @*/
{
    fd->fileSize = fileSize;
}

/*@unused@*/ static inline unsigned int fadGetFirstFree(FD_t fd) /*@*/ {
    return fd->firstFree;
}

/*@unused@*/ static inline void fadSetFirstFree(FD_t fd, unsigned int firstFree)
	/*@modifies fd @*/
{
    fd->firstFree = firstFree;
}

/** \ingroup db1
 */
/*@null@*/ FD_t	fadOpen(const char * path, int flags, mode_t perms)
	/*@modifies fileSystem @*/;

/** \ingroup db1
 * @param fd		file handle
 * @return		0 on failure
 */
unsigned int fadAlloc(FD_t fd, unsigned int size)
	/*@modifies fd, fileSystem @*/;

/** \ingroup db1
 * @param fd		file handle
 */
void fadFree(FD_t fd, unsigned int offset)
	/*@modifies fd, fileSystem @*/;

/** \ingroup db1
 * @param fd		file handle
 */
int fadFirstOffset(FD_t fd)
	/*@modifies fd, fileSystem @*/;

/** \ingroup db1
 * @param fd		file handle
 * @return		next offset, 0 to terminate
 */
int fadNextOffset(FD_t fd, unsigned int lastoff)
	/*@modifies fd, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_FALLOC */
