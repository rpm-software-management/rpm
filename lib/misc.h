#ifndef H_MISC
#define H_MISC

char ** splitString(char * str, int length, char sep);
void freeSplitString(char ** list);
void stripTrailingSlashes(char * str);

int exists(char * filespec);

int vercmp(char * one, char * two);

#endif
