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
typedef struct __dirstream *	DAVDIR;
#else
typedef DIR *			AVDIR;
typedef DIR *			DAVDIR;
#endif


/**
 */
/*@unchecked@*/
extern int avmagicdir;
#define ISAVMAGIC(_dir) (!memcmp((_dir), &avmagicdir, sizeof(avmagicdir)))

/**
 */
/*@unchecked@*/
extern int davmagicdir;
#define ISDAVMAGIC(_dir) (!memcmp((_dir), &davmagicdir, sizeof(davmagicdir)))

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
DIR * avOpendir(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * Send a http request.
 * @param data		
 * @param davCmd	http command
 * @param davArg	http command argumeny
 * @returns		0 on success
 */
int davReq(FD_t data, const char * davCmd, const char * davArg)
	/*@globals fileSystem, internalState @*/
	/*@modifies data, fileSystem, internalState @*/;

/**
 */
ssize_t davRead(void * cookie, /*@out@*/ char * buf, size_t count)
        /*@globals fileSystem, internalState @*/
        /*@modifies *buf, fileSystem, internalState @*/
        /*@requires maxSet(buf) >= (count - 1) @*/
        /*@ensures maxRead(buf) == result @*/;

/**
 */
ssize_t davWrite(void * cookie, const char * buf, size_t count)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/**
 */
int davSeek(void * cookie, _libio_pos_t pos, int whence)
        /*@globals fileSystem, internalState @*/
        /*@modifies fileSystem, internalState @*/;

/**
 */
int davClose(/*@only@*/ void * cookie)
	/*@globals fileSystem, internalState @*/
	/*@modifies cookie, fileSystem, internalState @*/;

/**
 * Close a DAV collection.
 * @param dir		argv DIR
 * @return 		0 always
 */
int davClosedir(/*@only@*/ DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies dir, fileSystem @*/;

/**
 * Return next entry from a DAV collection.
 * @param dir		argv DIR
 * @return 		next entry
 */
/*@dependent@*/ /*@null@*/
struct dirent * davReaddir(DIR * dir)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/**
 * Create an argv directory from DAV collection.
 * @param path		URL for DAV collection path
 * @return 		argv DIR
 */
/*@null@*/
DIR * davOpendir(const char * path)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/**
 * stat(2) clone.
 */
int davStat(const char * path, /*@out@*/ struct stat * st)
	/*@globals fileSystem, internalState @*/
	/*@modifies *st, fileSystem, internalState @*/;

/**
 * lstat(2) clone.
 */
int davLstat(const char * path, /*@out@*/ struct stat * st)
	/*@globals fileSystem, internalState @*/
	/*@modifies *st, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif /* RPMDAV_H */
