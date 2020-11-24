#include <stdio.h>

/*
 * Sources to reproduce libhello.so and hellolib:
 * $ gcc -DIAM_LIB -shared -fPIE -o libhello.so libhello.c
 * $ gcc -L$(pwd) libhello.c -lhello -o helloexe
 * $ gcc -L$(pwd) -pie libhello.c -lhello -o hellopie
 */

int libhello_world(FILE *f);

#ifdef IAM_LIB
int libhello_world(FILE *f)
{
    return fprintf(f, "hello world");
}
#else
int main(int argc, char *argv[])
{
    return libhello_world(stdout);
}
#endif
