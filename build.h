#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

int build(const char * arg, BTA_t ba, const char * passPhrase,
		char * cookie, /*@null@*/ const char * rcfile)
	/*@modifies ba->buildAmount, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif

