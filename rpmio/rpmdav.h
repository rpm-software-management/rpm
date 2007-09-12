#ifndef RPMDAV_H
#define RPMDAV_H

/** \ingroup rpmio
 * \file rpmio/rpmdav.h
 */

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* RPMDAV_H */
