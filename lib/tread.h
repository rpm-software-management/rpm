#ifndef H_TREAD
#define H_TREAD

#ifdef __cplusplus
extern "C" {
#endif

int timedRead(FD_t fd, void * bufptr, int length);

#ifdef __cplusplus
}
#endif

#endif	/* H_TREAD */
