#ifndef _FILES_H_
#define _FILES_H_

#include "spec.h"
#include "specP.h"
#include "stringbuf.h"

int process_filelist(Header header, struct PackageRec *pr, StringBuf sb,
		     int *size, char *name, char *version,
		     char *release, int type, char *prefix);

#endif _FILES_H_
