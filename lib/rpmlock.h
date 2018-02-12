#ifndef RPMLOCK_H
#define RPMLOCK_H 

#include <rpm/rpmutil.h>

typedef struct rpmlock_s * rpmlock;

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmlock rpmlockNew(const char *lock_path, const char *descr);

RPM_GNUC_INTERNAL
rpmlock rpmlockNewAcquire(const char *lock_path, const char *descr);

RPM_GNUC_INTERNAL
int rpmlockAcquire(rpmlock lock);

RPM_GNUC_INTERNAL
void rpmlockRelease(rpmlock lock);

RPM_GNUC_INTERNAL
rpmlock rpmlockFree(rpmlock lock);

#ifdef __cplusplus
}
#endif

#endif
