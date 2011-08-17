#ifndef H_MISC
#define H_MISC

/**
 * \file lib/misc.h
 *
 */

#include <string.h>
#include <rpm/rpmtypes.h>
#include <rpm/header.h>		/* for headerGetFlags typedef, duh.. */

#ifdef __cplusplus
extern "C" {
#endif

/* known arch? */
RPM_GNUC_INTERNAL
int rpmIsKnownArch(const char *name);

RPM_GNUC_INTERNAL
char * rpmVerifyString(uint32_t verifyResult, const char *pad);

RPM_GNUC_INTERNAL
char * rpmFFlagsString(uint32_t fflags, const char *pad);

typedef char * (*headerTagFormatFunction) (rpmtd td);
typedef int (*headerTagTagFunction) (Header h, rpmtd td, headerGetFlags hgflags);

RPM_GNUC_INTERNAL
headerTagTagFunction rpmHeaderTagFunc(rpmTagVal tag);

RPM_GNUC_INTERNAL
headerTagFormatFunction rpmHeaderFormatFuncByName(const char *fmt);

RPM_GNUC_INTERNAL
headerTagFormatFunction rpmHeaderFormatFuncByValue(rpmtdFormats fmt);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
