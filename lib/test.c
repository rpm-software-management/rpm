#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "header.h"
#include "rpmlib.h"

void main(int argc, char **argv)
{
    Header h, h2, h3, h4;
    int fd;
    char *sa[] = {"one", "two", "three"};
    int_32 i32 = 400;
    int_32 i32a[] = {100, 200, 300};
    int_16 i16 = 1;
    int_16 i16a[] = {100, 200, 300};
    char ca[] = "char array";

    h = newHeader();

    addEntry(h, RPMTAG_NAME, STRING_TYPE, "MarcEwing", 1);
    addEntry(h, RPMTAG_VERSION, STRING_TYPE, "1.1", 1);
    addEntry(h, RPMTAG_VERSION, STRING_ARRAY_TYPE, sa, 3);
    addEntry(h, RPMTAG_SIZE, INT32_TYPE, &i32, 1);
    addEntry(h, RPMTAG_SIZE, INT16_TYPE, &i16, 1);
    addEntry(h, RPMTAG_SIZE, INT16_TYPE, i16a, 3);
    addEntry(h, RPMTAG_VENDOR, CHAR_TYPE, ca, strlen(ca));
    addEntry(h, RPMTAG_SIZE, INT32_TYPE, i32a, 3);

    printf("Original = %d\n", sizeofHeader(h));
    fd = open("test.out", O_WRONLY|O_CREAT);
    writeHeader(fd, h);
    close(fd);
    h2 = copyHeader(h);
    printf("Copy     = %d\n", sizeofHeader(h2));

    fd = open("test.out", O_RDONLY);
    h3 = readHeader(fd);
    close(fd);
   
    printf("From disk    = %d\n", sizeofHeader(h3));
    h4 = copyHeader(h3);
    printf("Copy of disk = %d\n", sizeofHeader(h4));
   
    printf("=====================\n");
    printf("Original\n");
    dumpHeader(h, stdout, 1);
    printf("=====================\n");
    printf("From disk\n");
    dumpHeader(h3, stdout, 1);
    printf("=====================\n");
    printf("Copy of disk\n");
    dumpHeader(h4, stdout, 1);

#if 0
    convertDB("");
#endif
}
