#ifndef H_MISC
#define H_MISC

/**
 * \file misc.h
 *
 */

#include <string.h>
#include <rpm/rpmtypes.h>
#include <rpm/header.h>		/* for headerGetFlags typedef, duh.. */
#include "rpmfs.hh"

typedef const struct headerFmt_s * headerFmt;

/* known arch? */
int rpmIsKnownArch(const char *name);

char * rpmVerifyString(uint32_t verifyResult, const char *pad);

char * rpmFFlagsString(uint32_t fflags);

typedef int (*headerTagTagFunction) (Header h, rpmtd td, headerGetFlags hgflags);

headerTagTagFunction rpmHeaderTagFunc(rpmTagVal tag);

headerFmt rpmHeaderFormatByName(const char *fmt);

headerFmt rpmHeaderFormatByValue(rpmtdFormats fmt);

char * rpmHeaderFormatCall(headerFmt fmt, rpmtd td);

int headerFindSpec(Header h);

/**
 * Relocate files in header.
 * @todo multilib file dispositions need to be checked.
 * @param relocs		relocations
 * @param numRelocations	number of relocations
 * @param fs			file state set
 * @param h			package header to relocate
 */
void rpmRelocateFileList(rpmRelocation *relocs, int numRelocations, rpmfs fs, Header h);

int rpmRelocateSrpmFileList(Header h, const char *rootDir);

void rpmRelocationBuild(Header h, rpmRelocation *rawrelocs,
		int *rnrelocs, rpmRelocation **rrelocs, uint8_t **rbadrelocs);

int rpmIsValidHex(const char *str, size_t slen);
#endif	/* H_MISC */
