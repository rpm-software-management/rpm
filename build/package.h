#ifndef _H_PACKAGE_
#define _H_PACKAGE_

#ifdef __cplusplus
extern "C" {
#endif

int lookupPackage(Spec spec, char *name, int flag, Package *pkg);
Package newPackage(Spec spec);
void freePackages(Spec spec);
void freePackage(Package p);

#ifdef __cplusplus
}
#endif

#endif	/* _H_PACKAGE_ */
