#ifndef _MACRO_H_
#define _MACRO_H_

/* macro.h - %macro handling */

struct MacroEntry {
    char *name;
    char *expansion;
};

struct MacroContext {
    struct MacroEntry *macroTable;
    int macrosAllocated;
    int firstFree;
};

void initMacros(struct MacroContext *mc, const char *macrofile);
void freeMacros(struct MacroContext *mc);

void addMacro(struct MacroContext *mc, const char *n, const char *o, const char *b, int depth);

/* Expand all macros in buf, in place */
int expandMacros(Spec spec, struct MacroContext *mc, char *sbuf, size_t sbuflen);

#endif
