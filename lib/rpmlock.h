#ifndef RPMLOCK_H
#define RPMLOCK_H 

void *rpmtsAcquireLock(rpmts ts);
void rpmtsFreeLock(void *lock);

#endif
