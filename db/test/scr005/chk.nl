#!/bin/sh -
#
# Id: chk.nl,v 1.5 2001/10/12 17:55:33 bostic Exp 
#
# Check to make sure that there are no trailing newlines in __db_err calls.

d=../..

[ -f $d/README ] || {
	echo "FAIL: chk.nl can't find the source directory."
	exit 1
}

cat << END_OF_CODE > t.c
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>

int chk(FILE *, char *);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;
	int exitv;

	for (exitv = 0; *++argv != NULL;) {
		if ((fp = fopen(*argv, "r")) == NULL) {
			fprintf(stderr, "%s: %s\n", *argv, strerror(errno));
			return (1);
		}
		if (chk(fp, *argv))
			exitv = 1;
		(void)fclose(fp);
	}
	return (exitv);
}

int
chk(fp, name)
	FILE *fp;
	char *name;
{
	int ch, exitv, line, q;

	exitv = 0;
	for (ch = 'a', line = 1;;) {
		if ((ch = getc(fp)) == EOF)
			return (exitv);
		if (ch == '\n') {
			++line;
			continue;
		}
		if (ch != '_') continue;
		if ((ch = getc(fp)) != '_') continue;
		if ((ch = getc(fp)) != 'd') continue;
		if ((ch = getc(fp)) != 'b') continue;
		if ((ch = getc(fp)) != '_') continue;
		if ((ch = getc(fp)) != 'e') continue;
		if ((ch = getc(fp)) != 'r') continue;
		if ((ch = getc(fp)) != 'r') continue;
		while ((ch = getc(fp)) != '"') {
			if (ch == EOF)
				return (exitv);
			if (ch == '\n')
				++line;
		}
		while ((ch = getc(fp)) != '"') {
			if (ch == EOF)
				return (exitv);
			if (ch == '\n')
				++line;
			if (ch == '\\\\')
				if ((ch = getc(fp)) != 'n')
					ungetc(ch, fp);
				else if ((ch = getc(fp)) != '"')
					ungetc(ch, fp);
				else {
					fprintf(stderr,
				    "%s: <newline> at line %d\n", name, line);
					exitv = 1;
				}
		}
	}
	return (exitv);
}
END_OF_CODE

cc t.c -o t
if ./t $d/*/*.[ch] $d/*/*.cpp $d/*/*.in ; then
	:
else
	echo "FAIL: found __db_err calls with newline strings."
	exit 1
fi

exit 0
