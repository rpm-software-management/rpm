#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "db.h"

#define	E(api, func, name) {						\
	if ((ret = api(func)) != 0) {					\
		fprintf(stderr, "%s: %s", name, db_strerror(ret));	\
		return (1);						\
	}								\
}

void
dirfree(char **namesp, int cnt)
{ return; }
int
dirlist(const char *dir, char ***namesp, int *cntp)
{ return (0); }
int
exists(const char *path, int *isdirp)
{ return (0); }
int
ioinfo(const char *path,
    int fd, u_int32_t *mbytesp, u_int32_t *bytesp, u_int32_t *iosizep)
{ return (0); }
int
map(char *path, size_t len, int is_region, int is_rdonly, void **addr)
{ return (0); }
int
seek(int fd, off_t offset, int whence)
{ return (0); }
int
local_sleep(u_long seconds, u_long microseconds)
{ return (0); }
int
unmap(void *addr, size_t len)
{ return (0); }

int
main(int argc, char *argv[])
{
	int ret;

	E(db_env_set_func_close, close, "close");
	E(db_env_set_func_dirfree, dirfree, "dirfree");
	E(db_env_set_func_dirlist, dirlist, "dirlist");
	E(db_env_set_func_exists, exists, "exists");
	E(db_env_set_func_free, free, "free");
	E(db_env_set_func_fsync, fsync, "fsync");
	E(db_env_set_func_ftruncate, ftruncate, "ftruncate");
	E(db_env_set_func_ioinfo, ioinfo, "ioinfo");
	E(db_env_set_func_malloc, malloc, "malloc");
	E(db_env_set_func_map, map, "map");
	E(db_env_set_func_open, open, "open");
	E(db_env_set_func_pread, pread, "pread");
	E(db_env_set_func_pwrite, pwrite, "pwrite");
	E(db_env_set_func_read, read, "read");
	E(db_env_set_func_realloc, realloc, "realloc");
	E(db_env_set_func_rename, rename, "rename");
	E(db_env_set_func_seek, seek, "seek");
	E(db_env_set_func_sleep, local_sleep, "sleep");
	E(db_env_set_func_unlink, unlink, "unlink");
	E(db_env_set_func_unmap, unmap, "unmap");
	E(db_env_set_func_write, write, "write");
	E(db_env_set_func_yield, sched_yield, "yield");

	return (0);
}
