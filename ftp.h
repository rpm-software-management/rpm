#ifndef H_FTP
#define H_FTP

int ftpOpen(char * host, char * name, char * password);
int ftpGetFile(int sock, char * remotename, int dest);
void ftpClose(int sock);

#endif
