#ifndef H_BUILD
#define H_BUILD

#ifdef __cplusplus
extern "C" {
#endif

int build(rpmTransactionSet ts, const char * arg, BTA_t ba,
		/*@null@*/ const char * rcfile)
	/*@globals rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/
	/*@modifies ts, ba->buildAmount, rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif

