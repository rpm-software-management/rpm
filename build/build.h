#ifndef _BUILD_H_
#define _BUILD_H_

#include "spec.h"

int execPrep(Spec s);
int execBuild(Spec s);
int execInstall(Spec s);
int execClean(Spec s);
int verifyList(Spec s);

#endif _BUILD_H_
