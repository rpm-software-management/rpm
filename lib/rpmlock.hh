#ifndef RPMLOCK_H
#define RPMLOCK_H 

#include <rpm/rpmutil.h>

typedef struct rpmlock_s * rpmlock;

enum {
    RPMLOCK_READ   = 1 << 0,
    RPMLOCK_WRITE  = 1 << 1,
    RPMLOCK_WAIT   = 1 << 2,
};

rpmlock rpmlockNew(const char *lock_path, const char *descr);

rpmlock rpmlockNewAcquire(const char *lock_path, const char *descr);

int rpmlockAcquire(rpmlock lock, int lockmode = RPMLOCK_WRITE);

void rpmlockRelease(rpmlock lock);

rpmlock rpmlockFree(rpmlock lock);

#endif
