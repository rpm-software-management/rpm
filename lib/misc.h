#ifndef H_MISC
#define H_MISC

char ** splitString(char * str, int length, char sep);
void freeSplitString(char ** list);

int exists(char * filespec);

#endif
