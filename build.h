#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

int build(const char *arg, struct rpmBuildArguments *ba, const char *passPhrase,
	  int fromTarball, char *cookie, const char * rcfile, int force,
	  int nodeps);

#ifdef __cplusplus
}
#endif

#endif

