#ifndef H_MISC
#define H_MISC

char ** splitString(char * str, int length, char sep);
void freeSplitString(char ** list);
void stripTrailingSlashes(char * str);

int exists(char * filespec);

int rpmvercmp(char * one, char * two);

/* these are like the normal functions, but they malloc() the space which
   is needed */
int dosetenv(const char *name, const char *value, int overwrite);
int doputenv(const char * str);

#endif
