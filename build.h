#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

extern struct poptOption rpmBuildPoptTable[];

struct rpmBuildArguments {
    int buildAmount;
    char *buildRootOverride;
    char *targets;
    int useCatalog;
    int noLang;
    int noBuild;
    int shortCircuit;
    char buildChar;
};

int build(const char *arg, int buildAmount, const char *passPhrase,
	         const char *buildRoot, int fromTarball, int test, char *cookie,
                 const char * rcfile, char * buildplatforms, int force);

#ifdef __cplusplus
}
#endif

#endif

