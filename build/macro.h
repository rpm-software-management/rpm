/* macro.h - %macro handling */

void resetMacros(void);

void addMacro(char *name, char *expansion);

/* Expand all macros in buf, in place */
int expandMacros(char *buf);
