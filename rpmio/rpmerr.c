#include <stdarg.h>
#include <stdio.h>

#include "rpmerr.h"

static struct err {
    int code;
    char string[1024];
} errorRec;

static CallBackType errorCallback = NULL;

int errCode(void)
{
    return errorRec.code;
}

char *errCodeString(void)
{
    return NULL;
}

char *errString(void)
{
    return errorRec.string;
}

CallBackType errSetCallback(CallBackType cb)
{
    CallBackType ocb;

    ocb = errorCallback;
    errorCallback = cb;
    
    return ocb;
}

void error(int code, char *format, ...)
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
