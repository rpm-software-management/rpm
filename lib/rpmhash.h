#ifndef H_RPMHASH
#define H_RPMHASH

#include "rpm/rpmutil.h"

/**
 * \file lib/rpmhash.h
 * Hash table implemenation.
 */

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
unsigned int hashFunctionString(const char * string);

#ifdef __cplusplus
}
#endif

#endif
