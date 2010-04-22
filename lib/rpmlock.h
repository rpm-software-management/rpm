#ifndef RPMLOCK_H
#define RPMLOCK_H 

typedef struct rpmlock_s * rpmlock;

rpmlock rpmtsAcquireLock(rpmts ts);
void rpmtsFreeLock(rpmlock lock);

#endif
