#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

int build(rpmts ts, const char * arg, BTA_t ba,
		/*@null@*/ const char * rcfile)
	/*@globals rpmGlobalMacroContext, rpmCLIMacroContext,
		h_errno, fileSystem, internalState @*/
	/*@modifies ts, ba->buildAmount, rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif

