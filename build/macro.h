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

void initMacros(struct MacroContext *mc);
void freeMacros(struct MacroContext *mc);

void addMacro(struct MacroContext *mc, char *name, char *expansion);

/* Expand all macros in buf, in place */
int expandMacros(struct MacroContext *mc, char *buf);

#endif
