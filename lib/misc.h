#ifndef H_MISC
#define H_MISC

char ** splitString(char * str, int length, char sep);
void freeSplitString(char ** list);

int exists(char * filespec);

int getOsNum(void);
int getArchNum(void);
char *getOsName(void);
char *getArchName(void);

void initArchOs(char *arch, char *os);

int vercmp(char * one, char * two);

#endif
