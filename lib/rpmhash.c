/**
 * \file lib/rpmhash.c
 * Hash table implemenation
 */

#include "lib/rpmhash.h"

unsigned int hashFunctionString(const char * string)
{
    char xorValue = 0;
    char sum = 0;
    short len;
    int i;
    const char * chp = string;

    len = strlen(string);
    for (i = 0; i < len; i++, chp++) {
	xorValue ^= *chp;
	sum += *chp;
    }

    return ((((unsigned)len) << 16) + (((unsigned)sum) << 8) + xorValue);
}
