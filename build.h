#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

int build(const char *arg, struct rpmBuildArguments *ba, const char *passPhrase,
	  char *cookie, const char * rcfile);

#ifdef __cplusplus
}
#endif

#endif

