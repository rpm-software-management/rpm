#ifndef H_HTTP
#define H_HTTP

int httpProxySetup(const char * url, urlinfo ** uret);
int httpCheckResponse(int fd, char ** str);
int httpSkipHeader(FD_t sfd, char *buf,int * bytesRead, char ** start);



#define HTTPERR_BAD_SERVER_RESPONSE   -1
#define HTTPERR_SERVER_IO_ERROR       -2
#define HTTPERR_SERVER_TIMEOUT        -3
#define HTTPERR_BAD_HOSTNAME          -5
/*
#define FTPERR_BAD_HOST_ADDR         -4
#define FTPERR_FAILED_CONNECT        -6
#define FTPERR_FILE_IO_ERROR         -7
#define FTPERR_PASSIVE_ERROR         -8
#define FTPERR_FAILED_DATA_CONNECT   -9
#define FTPERR_FILE_NOT_FOUND        -10
#define FTPERR_NIC_ABORT_IN_PROGRESS -11
*/
#define HTTPERR_UNKNOWN               -100

#endif
