#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "misc.h"
#include "spec.h"
#include "rpmlib.h"
#include "header.h"
#include "popt/popt.h"

void addOrAppendListEntry(Header h, int_32 tag, char *line)
{
    int argc;
    char **argv;

    poptParseArgvString(line, &argc, &argv);
    if (argc) {
	headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, argv, argc);
    }
    FREE(argv);
}

/* Parse a simple part line that only take -n <pkg> or <pkg> */
/* <pkg> is return in name as a pointer into a static buffer */

int parseSimplePart(char *line, char **name, int *flag)
{
    char *tok;
    char linebuf[BUFSIZ];
    static char buf[BUFSIZ];

    strcpy(linebuf, line);

    /* Throw away the first token (the %xxxx) */
    strtok(linebuf, " \t\n");
    
    if (!(tok = strtok(NULL, " \t\n"))) {
	*name = NULL;
	return 0;
    }
    
    if (!strcmp(tok, "-n")) {
	if (!(tok = strtok(NULL, " \t\n"))) {
	    return 1;
	}
	*flag = PART_NAME;
    } else {
	*flag = PART_SUBNAME;
    }
    strcpy(buf, tok);
    *name = buf;

    return (strtok(NULL, " \t\n")) ? 1 : 0;
}

int parseYesNo(char *s)
{
    if (!s || (s[0] == 'n' || s[0] == 'N') ||
	!strcasecmp(s, "false") ||
	!strcasecmp(s, "off") ||
	!strcmp(s, "0")) {
	return 0;
    }

    return 1;
}

char *findLastChar(char *s)
{
    char *res = s;

    while (*s) {
	if (! isspace(*s)) {
	    res = s;
	}
	s++;
    }

    return res;
}

int parseNum(char *line, int *res)
{
    char *s1;
    
    s1 = NULL;
    *res = strtoul(line, &s1, 10);
    if ((*s1) || (s1 == line) || (*res == ULONG_MAX)) {
	return 1;
    }

    return 0;
}

StringBuf getOutputFrom(char *dir, char *argv[],
			char *writePtr, int writeBytesLeft,
			int failNonZero)
{
    int progPID;
    int progDead;
    int toProg[2];
    int fromProg[2];
    int status;
    void *oldhandler;
    int bytesWritten;
    StringBuf readBuff;
    int bytes;
    unsigned char buf[8193];

    oldhandler = signal(SIGPIPE, SIG_IGN);

    pipe(toProg);
    pipe(fromProg);
    
    if (!(progPID = fork())) {
	close(toProg[1]);
	close(fromProg[0]);
	
	dup2(toProg[0], 0);   /* Make stdin the in pipe */
	dup2(fromProg[1], 1); /* Make stdout the out pipe */

	close(toProg[0]);
	close(fromProg[1]);

	if (dir) {
	    chdir(dir);
	}
	
	execvp(argv[0], argv);
	rpmError(RPMERR_EXEC, "Couldn't exec %s", argv[0]);
	_exit(RPMERR_EXEC);
    }
    if (progPID < 0) {
	rpmError(RPMERR_FORK, "Couldn't fork %s", argv[0]);
	return NULL;
    }

    close(toProg[0]);
    close(fromProg[1]);

    /* Do not block reading or writing from/to prog. */
    fcntl(fromProg[0], F_SETFL, O_NONBLOCK);
    fcntl(toProg[1], F_SETFL, O_NONBLOCK);
    
    readBuff = newStringBuf();

    progDead = 0;
    do {
	if (waitpid(progPID, &status, WNOHANG)) {
	    progDead = 1;
	}

	/* Write some stuff to the process if possible */
        if (writeBytesLeft) {
	    if ((bytesWritten =
		  write(toProg[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("getOutputFrom()");
	            exit(1);
		}
	        bytesWritten = 0;
	    }
	    writeBytesLeft -= bytesWritten;
	    writePtr += bytesWritten;
	} else {
	    close(toProg[1]);
	}
	
	/* Read any data from prog */
	bytes = read(fromProg[0], buf, sizeof(buf)-1);
	while (bytes > 0) {
	    buf[bytes] = '\0';
	    appendStringBuf(readBuff, buf);
	    bytes = read(fromProg[0], buf, sizeof(buf)-1);
	}

	/* terminate when prog dies */
    } while (!progDead);

    close(toProg[1]);
    close(fromProg[0]);
    signal(SIGPIPE, oldhandler);

    if (writeBytesLeft) {
	rpmError(RPMERR_EXEC, "failed to write all data to %s", argv[0]);
	return NULL;
    }
    waitpid(progPID, &status, 0);
    if (failNonZero && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	rpmError(RPMERR_EXEC, "%s failed", argv[0]);
	return NULL;
    }

    return readBuff;
}

char *cleanFileName(char *name)
{
    static char res[BUFSIZ];
    char *copyTo, *copyFrom, copied;

    /* Copy to fileName, eliminate duplicate "/" and trailing "/" */
    copyTo = res;
    copied = '\0';
    copyFrom = name;
    while (*copyFrom) {
	if (*copyFrom != '/' || copied != '/') {
	    *copyTo++ = copied = *copyFrom;
	}
	copyFrom++;
    }
    *copyTo = '\0';
    copyTo--;
    if ((copyTo != res) && (*copyTo == '/')) {
	*copyTo = '\0';
    }

    return res;
}
