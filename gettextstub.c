/* this is so we can link RPM without -lintl. It makes it about 10k
   smaller, so its worth doing for tight corners (like install time) */

void bindtextdomain(char * a, char * b);
void textdomain(char * a);
char * gettext(char * a);

void bindtextdomain(char * a, char * b) {
}

char * gettext(char * a) {
    return a;
}

void textdomain(char * a) {
}
