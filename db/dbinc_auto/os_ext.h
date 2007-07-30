/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_os_ext_h_
#define	_os_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

void __os_abort __P((void));
int __os_abspath __P((const char *));
int __os_umalloc __P((DB_ENV *, size_t, void *));
int __os_urealloc __P((DB_ENV *, size_t, void *));
void __os_ufree __P((DB_ENV *, void *));
int __os_strdup __P((DB_ENV *, const char *, void *));
int __os_calloc __P((DB_ENV *, size_t, size_t, void *));
int __os_malloc __P((DB_ENV *, size_t, void *));
int __os_realloc __P((DB_ENV *, size_t, void *));
void __os_free __P((DB_ENV *, void *));
void *__ua_memcpy __P((void *, const void *, size_t));
void __os_gettime __P((DB_ENV *, db_timespec *));
int __os_fs_notzero __P((void));
int __os_support_direct_io __P((void));
int __os_support_db_register __P((void));
int __os_support_replication __P((void));
int __os_dirlist __P((DB_ENV *, const char *, char ***, int *));
void __os_dirfree __P((DB_ENV *, char **, int));
int __os_get_errno_ret_zero __P((void));
int __os_get_errno __P((void));
int __os_get_neterr __P((void));
int __os_get_syserr __P((void));
void __os_set_errno __P((int));
char *__os_strerror __P((int, char *, size_t));
int __os_posix_err __P((int));
int __os_fileid __P((DB_ENV *, const char *, int, u_int8_t *));
int __os_fdlock __P((DB_ENV *, DB_FH *, off_t, int, int));
int __os_fsync __P((DB_ENV *, DB_FH *));
int __os_zerofill __P((DB_ENV *, DB_FH *));
int __os_getenv __P((DB_ENV *, const char *, char **, size_t));
int __os_openhandle __P((DB_ENV *, const char *, int, int, DB_FH **));
int __os_closehandle __P((DB_ENV *, DB_FH *));
int __os_r_sysattach __P((DB_ENV *, REGINFO *, REGION *));
int __os_r_sysdetach __P((DB_ENV *, REGINFO *, int));
int __os_mapfile __P((DB_ENV *, char *, DB_FH *, size_t, int, void **));
int __os_unmapfile __P((DB_ENV *, void *, size_t));
int __os_mkdir __P((DB_ENV *, const char *, int));
u_int32_t __db_oflags __P((int));
int __db_omode __P((const char *));
int __os_open __P((DB_ENV *, const char *, u_int32_t, u_int32_t, int, DB_FH **));
void __os_id __P((DB_ENV *, pid_t *, db_threadid_t*));
int __os_r_attach __P((DB_ENV *, REGINFO *, REGION *));
int __os_r_detach __P((DB_ENV *, REGINFO *, int));
int __os_rename __P((DB_ENV *, const char *, const char *, u_int32_t));
int __os_isroot __P((void));
char *__db_rpath __P((const char *));
int __os_io __P((DB_ENV *, int, DB_FH *, db_pgno_t, u_int32_t, u_int32_t, u_int32_t, u_int8_t *, size_t *));
int __os_read __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
int __os_write __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
int __os_physwrite __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
int __os_seek __P((DB_ENV *, DB_FH *, db_pgno_t, u_int32_t, u_int32_t));
void __os_sleep __P((DB_ENV *, u_long, u_long));
u_int32_t __os_spin __P((DB_ENV *));
int __os_exists __P((DB_ENV *, const char *, int *));
int __os_ioinfo __P((DB_ENV *, const char *, DB_FH *, u_int32_t *, u_int32_t *, u_int32_t *));
int __os_tmpdir __P((DB_ENV *, u_int32_t));
int __os_truncate __P((DB_ENV *, DB_FH *, db_pgno_t, u_int32_t));
void __os_unique_id __P((DB_ENV *, u_int32_t *));
int __os_region_unlink __P((DB_ENV *, const char *));
int __os_unlink __P((DB_ENV *, const char *));
void __os_yield __P((DB_ENV *));
#ifndef HAVE_FCLOSE
int fclose __P((FILE *));
#endif
#ifndef HAVE_FGETC
int fgetc __P((FILE *));
#endif
#ifndef HAVE_FGETS
char *fgets __P((char *, int, FILE *));
#endif
#ifndef HAVE_FOPEN
FILE *fopen __P((const char *, const char *));
#endif
#ifndef HAVE_FWRITE
size_t fwrite __P((const void *, size_t, size_t, FILE *));
#endif
#ifndef HAVE_LOCALTIME
struct tm *localtime __P((const time_t *));
#endif
#ifndef HAVE_TIME
time_t time __P((time_t *));
#endif
#ifdef HAVE_QNX
int __os_qnx_region_open __P((DB_ENV *, const char *, int, int, DB_FH *));
#endif
#ifdef HAVE_QNX
int __os_qnx_shmname __P((DB_ENV *, const char *, char **));
#endif
#ifndef HAVE_TIME
time_t time __P((time_t *));
#endif
int __os_is_winnt __P((void));
#ifdef HAVE_REPLICATION_THREADS
int __os_get_neterr __P((void));
#endif
int __os_mkdir __P((DB_ENV *, const char *, int));

#if defined(__cplusplus)
}
#endif
#endif /* !_os_ext_h_ */
