#ifndef	_H_BUILD_
#define	_H_BUILD_

#include "spec.h"

#define RPMBUILD_PREP             (1 << 0)
#define RPMBUILD_BUILD            (1 << 1)
#define RPMBUILD_INSTALL          (1 << 2)
#define RPMBUILD_CLEAN            (1 << 3)
#define RPMBUILD_FILECHECK        (1 << 4)
#define RPMBUILD_PACKAGESOURCE    (1 << 5)
#define RPMBUILD_PACKAGEBINARY    (1 << 6)
#define RPMBUILD_RMSOURCE         (1 << 7)
#define RPMBUILD_RMBUILD          (1 << 8)
#define RPMBUILD_STRINGBUF        (1 << 9) /* only for doScript() */

#ifdef __cplusplus
extern "C" {
#endif

int buildSpec(Spec spec, int what, int test);
int doScript(Spec spec, int what, char *name, StringBuf sb, int test);

#ifdef __cplusplus
}
#endif

#endif	/* _H_BUILD_ */
