#ifndef H_FINGERPRINT
#define H_FINGERPRINT

typedef struct fingerprint_s {
    dev_t dev;
    ino_t ino;
    char * basename;
} fingerPrint;

/* Be carefull with the memory... assert(*fullName == '/' || !scareMemory) */
fingerPrint fpLookup(char * fullName, int scareMemory);

/* only if !scarceMemory */
#define fpFree(a) free((a).basename)

#define FP_EQUAL(a, b) (((a).dev == (b).dev) && \
			((a).ino == (b).ino) && \
			!strcmp((a).basename, (b).basename))

#endif
