/*
gmo2msg.c - create X/Open message source file for libelf.
Copyright (C) 1996 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <libintl.h>

#define DOMAIN	"libelf"

static const char *const msgs[] = {
#define __err__(a,b)	b,
#include <errors.h>
#undef __err__
};

int
main(int argc, char **argv) {
    char buf[1024], *lang, *progname, *s;
    unsigned i;

    if (*argv && (progname = strrchr(argv[0], '/'))) {
	progname++;
    }
    else if (!(progname = *argv)) {
	progname = "gmo2msg";
    }

    if (!(lang = getenv("LANGUAGE"))
     && !(lang = getenv("LC_ALL"))
     && !(lang = getenv("LC_MESSAGES"))
     && !(lang = getenv("LANG"))) {
	fprintf(stderr, "LANG variable not set.\n");
	exit(1);
    }

    /*
     * Fool gettext...
     */
    getcwd(buf, sizeof(buf));
    bindtextdomain(DOMAIN, buf);
    sprintf(buf, "%s.gmo", lang);
    unlink(DOMAIN ".mo"); symlink(buf, DOMAIN ".mo");
    unlink("LC_MESSAGES"); symlink(".", "LC_MESSAGES");
    unlink(lang); symlink(".", lang);
    printf("$set 1 # Automatically created from %s by %s\n", buf, progname);

    /*
     * Get translated messages.
     * Assume that messages contain printable ASCII characters ONLY.
     * That means no tabs, linefeeds etc.
     */
    textdomain(DOMAIN);
    for (i = 0; i < sizeof(msgs)/sizeof(*msgs); i++) {
	s = gettext(msgs[i]);
	if (s != msgs[i] && strcmp(s, msgs[i])) {
	    printf("$ Original message: %s\n", msgs[i]);
	    printf("%u %s\n", i + 1, s);
	}
    }

    /*
     * Cleanup.
     */
    unlink(DOMAIN ".mo");
    unlink("LC_MESSAGES");
    unlink(lang);
    exit(0);
}
