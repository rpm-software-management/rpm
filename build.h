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

int build(char *arg, int buildAmount, char *passPhrase,
	         char *buildRoot, int fromTarball, int test, char *cookie,
                 char * rcfile, char * buildplatforms, int force);

#ifdef __cplusplus
}
#endif

#endif

