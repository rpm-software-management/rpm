#ifndef RPMHOOK_H
#define RPMHOOK_H

#include <vector>

typedef union {
    const char * s;
    int i;
    double f;
    void * p;
} rpmhookArgv;

typedef struct rpmhookArgs_s {
    int argc;
    const char * argt;
    std::vector<rpmhookArgv> argv;
} * rpmhookArgs;

typedef int (*rpmhookFunc) (rpmhookArgs args, void *data);

rpmhookArgs rpmhookArgsNew(int argc);
rpmhookArgs rpmhookArgsFree(rpmhookArgs args);

void rpmhookRegister(const char *name, rpmhookFunc func, void *data);
void rpmhookUnregister(const char *name, rpmhookFunc func, void *data);
void rpmhookUnregisterAny(const char *name, rpmhookFunc func);
void rpmhookUnregisterAll(const char *name);
void rpmhookCall(const char *name, const char *argt, ...);
void rpmhookCallArgs(const char *name, rpmhookArgs args);

#endif
