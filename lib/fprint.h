#ifndef H_FINGERPRINT
#define H_FINGERPRINT

typedef struct fingerprint_s {
    dev_t dev;
    ino_t ino;
    const char * basename;
} fingerPrint;

/* Be carefull with the memory... assert(*fullName == '/' || !scareMemory) */
fingerPrint fpLookup(const char * fullName, int scareMemory);
unsigned int fpHashFunction(const void * string);
int fpEqual(const void * key1, const void * key2);
/* scareMemory is assumed here! */
void fpLookupList(const char ** fullNames, fingerPrint * fpList, int numItems,
		  int alreadySorted);

/* only if !scarceMemory */
#define fpFree(a) free((void *)(a).basename)

#define FP_EQUAL(a, b) ((&(a) == &(b)) || \
			       (((a).dev == (b).dev) && \
			        ((a).ino == (b).ino) && \
			        !strcmp((a).basename, (b).basename)))

#endif
