#ifndef RPMLOCK_H
#define RPMLOCK_H 

/*@only@*/ /*@null@*/
void * rpmtsAcquireLock(rpmts ts)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;
void rpmtsFreeLock(/*@only@*/ /*@null@*/ void *lock)
	/*@globals fileSystem, internalState @*/
	/*@modifies lock, fileSystem, internalState @*/;

#endif
