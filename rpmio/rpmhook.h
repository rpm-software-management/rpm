#ifndef RPMHOOK_H
#define RPMHOOK_H

typedef union {
    char *s;
    int i;
    float f;
    void *p;
} rpmhookArgv;

typedef struct rpmhookArgs_s {
    int argc;
    const char *argt;
    rpmhookArgv argv[1];
} * rpmhookArgs;

typedef int (*rpmhookFunc)(rpmhookArgs args, void *data);

void rpmhookRegister(const char *name, rpmhookFunc func, void *data);
void rpmhookUnregister(const char *name, rpmhookFunc func, void *data);
void rpmhookUnregisterAny(const char *name, rpmhookFunc func);
void rpmhookUnregisterAll(const char *name);
void rpmhookCall(const char *name, const char *argt, ...);

#endif

/* vim:ts=4:sw=4:et
 */
