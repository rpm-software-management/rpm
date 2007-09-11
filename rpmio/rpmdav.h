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

#if !defined(DT_DIR) || defined(__APPLE__)
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
extern int avmagicdir;
#define ISAVMAGIC(_dir) (!memcmp((_dir), &avmagicdir, sizeof(avmagicdir)))

/**
 */
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
int avClosedir(DIR * dir);

/**
 * Return next entry from an argv directory.
 * @param dir		argv DIR
 * @return 		next entry
 */
struct dirent * avReaddir(DIR * dir);

/**
 * Create an argv directory from URL collection.
 * @param path		URL for collection path
 * @return 		argv DIR
 */
DIR * avOpendir(const char * path);

/**
 * Send a http request.
 * @param ctrl		
 * @param httpCmd	http command
 * @param httpArg	http command argument (NULL if none)
 * @returns		0 on success
 */
int davReq(FD_t ctrl, const char * httpCmd, const char * httpArg);

/**
 * Read a http response.
 * @param u
 * @param cntl		
 * @retval *str		error msg		
 * @returns		0 on success
 */
int davResp(urlinfo u, FD_t ctrl, char *const * str);

/**
 */
FD_t davOpen(const char * url, int flags,
		mode_t mode, urlinfo * uret);

/**
 */
ssize_t davRead(void * cookie, char * buf, size_t count);

/**
 */
ssize_t davWrite(void * cookie, const char * buf, size_t count);

/**
 */
int davSeek(void * cookie, _libio_pos_t pos, int whence);

/**
 */
int davClose(void * cookie);

/**
 */
int davMkdir(const char * path, mode_t mode);

/**
 */
int davRmdir(const char * path);

/**
 */
int davRename(const char * oldpath, const char * newpath);

/**
 */
int davUnlink(const char * path);

/**
 * Close a DAV collection.
 * @param dir		argv DIR
 * @return 		0 always
 */
int davClosedir(DIR * dir);

/**
 * Return next entry from a DAV collection.
 * @param dir		argv DIR
 * @return 		next entry
 */
struct dirent * davReaddir(DIR * dir);

/**
 * Create an argv directory from DAV collection.
 * @param path		URL for DAV collection path
 * @return 		argv DIR
 */
DIR * davOpendir(const char * path);

/**
 * stat(2) clone.
 */
int davStat(const char * path, struct stat * st);

/**
 * lstat(2) clone.
 */
int davLstat(const char * path, struct stat * st);

#ifdef __cplusplus
}
#endif

#endif /* RPMDAV_H */
