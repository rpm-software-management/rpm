#include "system.h"

/** \ingroup rpmio
 * \file rpmio/rpmerr.c
 */

#include <stdarg.h>

#include <rpmerr.h>
/*@access rpmerrRec @*/

static struct rpmerrRec_s _rpmerrRec;

static rpmErrorCallBackType errorCallback = NULL;

int rpmErrorCode(void)
{
    return _rpmerrRec.code;
}

const char *rpmErrorCodeString(rpmerrCode code)
{
    return NULL;
}

char *rpmErrorString(void)
{
    return _rpmerrRec.string;
}

rpmErrorCallBackType rpmErrorSetCallback(rpmErrorCallBackType cb)
{
    rpmErrorCallBackType ocb;

    ocb = errorCallback;
    errorCallback = cb;
    
    return ocb;
}

void rpmError(rpmerrCode code, const char *format, ...)
{
    va_list args;

    va_start(args, format);

    _rpmerrRec.code = code;
    vsprintf(_rpmerrRec.string, format, args);

    if (errorCallback) {
	errorCallback();
    } else {
	fputs(_rpmerrRec.string, stderr);
	fputs("\n", stderr);
    }
}
