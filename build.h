#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

extern struct poptOption rpmBuildPoptTable[];

struct rpmBuildArguments {
    int buildAmount;
    const char *buildRootOverride;
    char *targets;
    int useCatalog;
    int noLang;
    int noBuild;
    int shortCircuit;
    char buildChar;
    /*@dependent@*/ const char *rootdir;
};

int build(const char *arg, struct rpmBuildArguments *ba, const char *passPhrase,
	  int fromTarball, char *cookie, const char * rcfile, int force,
	  int nodeps);

#ifdef __cplusplus
}
#endif

#endif

