#include "system.h"

#include <stdarg.h>

#include <rpmerr.h>

static struct err {
    int code;
    char string[1024];
} errorRec;

static rpmErrorCallBackType errorCallback = NULL;

int rpmErrorCode(void)
{
    return errorRec.code;
}

char *rpmErrorCodeString(void)
{
    return NULL;
}

char *rpmErrorString(void)
{
    return errorRec.string;
}

rpmErrorCallBackType rpmErrorSetCallback(rpmErrorCallBackType cb)
{
    rpmErrorCallBackType ocb;

    ocb = errorCallback;
    errorCallback = cb;
    
    return ocb;
}

void rpmError(int code, char *format, ...)
{
    va_list args;

    va_start(args, format);

    errorRec.code = code;
    vsprintf(errorRec.string, format, args);

    if (errorCallback) {
	errorCallback();
    } else {
	fputs(errorRec.string, stderr);
	fputs("\n", stderr);
    }
}
