/* This is really, really dumb. But AmigaOS gives us vfork(), but not
   fork() and this should make things work despite their brokenness */

#include <unistd.h>

int fork() {
    return vfork();
}
