#ifndef _PACKAGE_H_
#define _PACKAGE_H_

int lookupPackage(Spec spec, char *name, int flag, Package *pkg);
Package newPackage(Spec spec);
void freePackages(Spec spec);
void freePackage(Package p);

#endif
