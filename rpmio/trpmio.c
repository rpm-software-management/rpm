/** \ingroup rpmio
 * \file rpmio/trpmio.c
 */

#include <stdio.h>
#include "rpmio.h"
#include "debug.h"

int main (void)
{
    FD_t f1, f2, f3, f4, f5;
 
    printf ("open http://www.gnome.org/\n");
    f1 = Fopen ("http://www.gnome.org/", "r.fdio");
 
    printf ("open http://people.redhat.com/\n");
    f2 = Fopen ("http://people.redhat.com/", "r.ufdio");
 
    printf ("close http://www.gnome.org/\n");
    Fclose (f1);
 
    printf ("open http://www.redhat.com/\n");
    f3 = Fopen ("http://www.redhat.com/", "r.ufdio");
 
    printf ("close http://people.redhat.com/\n");
    Fclose (f2);
 
    printf ("open http://www.slashdot.org/\n");
    f4 = Fopen ("http://www.slashdot.org/", "r.ufdio");
 
    printf ("close http://people.redhat.com/\n");
    Fclose (f3);
 
    printf ("open http://people.redhat.com/\n");
    f5 = Fopen ("http://people.redhat.com/", "r.ufdio");
 
    printf ("close http://www.slashdot.org/\n");
    Fclose (f4);
 
    printf ("close http://people.redhat.com/\n");
    Fclose (f5);
 
    return 0;
}
