#ifndef RPMDAV_H
#define RPMDAV_H

/** \ingroup rpmio
 * \file rpmio/rpmdav.h
 */

#if defined(_RPMDAV_INTERNAL)
struct __dirstream {
    int fd;			/* File descriptor.  */
    char * data;		/* Directory block.  */
    size_t allocation;		/* Space allocated for the block.  */
    size_t size;		/* Total valid data in the block.  */
    size_t offset;		/* Current offset into the block.  */
    off_t filepos;		/* Position of next entry to read.  */
    pthread_mutex_t lock;	/* Mutex lock for this structure.  */
};
#endif

#if !defined(DT_DIR)
# define DT_UNKNOWN	0
# define DT_FIFO	1
# define DT_CHR		2
# define DT_DIR		4
# define DT_BLK		6
# define DT_REG		8
# define DT_LNK		10
# define DT_SOCK	12
# define DT_WHT		14
typedef struct __dirstream *	AVDIR;
#else
typedef DIR *			AVDIR;
#endif


/**
 */
/*@unchecked@*/
extern int avmagicdir;
#define ISAVMAGIC(_dir) (!memcmp((_dir), &avmagicdir, sizeof(avmagicdir)))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Close an argv directory.
 * @param dir		argv DIR
 * @return 		0 always
 */
int avClosedir(/*@only@*/ DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies dir, fileSystem @*/;

/**
 * Return next entry from an argv directory.
 * @param dir		argv DIR
 * @return 		next entry
 */
/*@dependent@*/ /*@null@*/
struct dirent * avReaddir(DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/**
 * Create an argv directory from URL collection.
 * @param path		URL for collection path
 * @return 		argv DIR
 */
/*@null@*/
static DIR * avOpendir(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Create an argv directory from DAV collection.
 * @param path		URL for DAV collection path
 * @return 		argv DIR
 */
/*@null@*/
static DIR * davOpendir(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMDAV_H */
