#ifndef _BUILD_H_
#define _BUILD_H_

#include "spec.h"

int doBuild(Spec s, int flags, char *passPhrase);
int execPrep(Spec s, int really_exec);
int execBuild(Spec s);
int execInstall(Spec s);
int execClean(Spec s);
int verifyList(Spec s);

extern char build_subdir[1024];

#define RPMBUILD_PREP        1
#define RPMBUILD_BUILD      (1 << 1)
#define RPMBUILD_INSTALL    (1 << 2)
#define RPMBUILD_BINARY     (1 << 3)
#define RPMBUILD_SOURCE     (1 << 4)
#define RPMBUILD_SWEEP      (1 << 5)
#define RPMBUILD_LIST       (1 << 6)
#define RPMBUILD_RMSOURCE   (1 << 7)

#endif _BUILD_H_
