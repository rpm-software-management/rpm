#ifndef _PARSE_H_
#define _PARSE_H_

int parseChangelog(Spec spec);
int parseDescription(Spec spec);
int parseFiles(Spec spec);
int parsePreamble(Spec spec, int initialPackage);
int parsePrep(Spec spec);
int parseRequiresConflicts(Spec spec, Package pkg, char *field,
			   int tag, int index);
int parseProvidesObsoletes(Spec spec, Package pkg, char *field, int tag);
int parseTrigger(Spec spec, Package pkg, char *field, int tag);
int parseScript(Spec spec, int parsePart);
int parseBuildInstallClean(Spec spec, int parsePart);

int parseSpec(Spec *specp, char *specFile, char *buildRoot,
	      int inBuildArch, char *passPhrase, char *cookie);

#endif
