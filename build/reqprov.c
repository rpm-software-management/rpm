/* reqprov.c -- require/provide handling */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include "specP.h"
#include "reqprov.h"
#include "messages.h"
#include "rpmlib.h"
#include "rpmerr.h"
#include "misc.h"

static StringBuf getOutputFrom(char *dir, char *argv[],
			       char *writePtr, int writeBytesLeft);

/*************************************************************/
/*                                                           */
/* Adding entries to the package reqprov list                */
/* Handle duplicate entries                                  */
/*                                                           */
/*************************************************************/

int addReqProv(struct PackageRec *p, int flags,
	       char *name, char *version)
{
    struct ReqProv *rd;
    int same;

    /* Frist see if the same entry is already there */
    rd = p->reqprov;
    while (rd) {
	if (rd->flags == flags) {
	    if (rd->version == version) {
		same = 1;
	    } else if (!rd->version || !version) {
		same = 0;
	    } else if (!strcmp(rd->version, version)) {
		same = 1;
	    } else {
		same = 0;
	    }

	    if (same && !strcmp(rd->name, name)) {
		/* They are exacty the same */
		break;
	    }
	}
	rd = rd->next;
    }
    if (rd) {
	/* already there */
	message(MESS_DEBUG, "Already Got: %s\n", name);
	return 0;
    }
    
    rd = (struct ReqProv *)malloc(sizeof(*rd));
    rd->flags = flags;
    rd->name = strdup(name);
    rd->version = version ? strdup(version) : NULL;
    rd->next = p->reqprov;
    p->reqprov = rd;

    if (flags & REQUIRE_PROVIDES) {
	message(MESS_DEBUG, "Adding provide: %s\n", name);
	p->numProv++;
    } else if (flags & REQUIRE_CONFLICTS) {
	message(MESS_DEBUG, "Adding conflict: %s\n", name);
	p->numConflict++;
    } else {
	message(MESS_DEBUG, "Adding require: %s\n", name);
	p->numReq++;
    }

    return 0;
}

/*************************************************************/
/*                                                           */
/* Add require/provides for the files in the header          */
/*  (adds to the package structure)                          */
/*                                                           */
/*************************************************************/

static StringBuf getOutputFrom(char *dir, char *argv[],
			       char *writePtr, int writeBytesLeft)
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
	close(0);
	close(1);
	close(toProg[1]);
	close(fromProg[0]);
	
	dup2(toProg[0], 0);   /* Make stdin the in pipe */
	dup2(fromProg[1], 1); /* Make stdout the out pipe */
	close(2);             /* Toss stderr */

	chdir(dir);
	
	execvp(argv[0], argv);
	error(RPMERR_EXEC, "Couldn't exec %s", argv[0]);
	exit(RPMERR_EXEC);
    }
    if (progPID < 0) {
	error(RPMERR_FORK, "Couldn't fork %s", argv[0]);
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
	error(RPMERR_EXEC, "failed to write all data to %s", argv[0]);
	return NULL;
    }
    waitpid(progPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_EXEC, "%s failed", argv[0]);
	return NULL;
    }

    return readBuff;
}

int generateAutoReqProv(Header header, struct PackageRec *p)
{
    char **f, **fsave, *s, *tok;
    int count;
    int_16 *modes;

    StringBuf writeBuff;
    StringBuf readBuff;
    char *writePtr;
    int writeBytes;
    char dir[1024];
    char *argv[8];

    message(MESS_VERBOSE, "Finding dependencies...\n");

    /*** Get root directory ***/
    
    if (getVar(RPMVAR_ROOT)) {
	strcpy(dir, getVar(RPMVAR_ROOT));
    } else {
	strcpy(dir, "/");
    }

    /*** Generate File List ***/
    
    if (!getEntry(header, RPMTAG_FILENAMES, NULL, (void **) &f, &count)) {
	return 0;
    }
    if (!count) {
	return 0;
    }
    fsave = f;
    getEntry(header, RPMTAG_FILEMODES, NULL, (void **) &modes, NULL);

    writeBuff = newStringBuf();
    writeBytes = 0;
    while (count--) {
        s = *f++;
	if (S_ISREG(*modes++)) {
	    /* We skip the leading "/" (already normalized) */
	    writeBytes += strlen(s);
	    appendLineStringBuf(writeBuff, s + 1);
	}
    }
    if (fsave) {
	free(fsave);
    }
    writePtr = getStringBuf(writeBuff);

    /*** Do Provides ***/
    
    argv[0] = "find-provides";
    argv[1] = NULL;
    readBuff = getOutputFrom(dir, argv, writePtr, writeBytes);
    if (!readBuff) {
	error(RPMERR_EXEC, "Failed to find provides");
	exit(1);
    }
    
    s = getStringBuf(readBuff);
    f = fsave = splitString(s, strlen(s), '\n');
    freeStringBuf(readBuff);
    while (*f) {
	if (**f) {
	    addReqProv(p, REQUIRE_PROVIDES, *f, NULL);
	}
	f++;
    }
    free(fsave);

    /*** Do Requires ***/
    
    /* Separate args by null for xargs (forget why) */
    s = writePtr;
    while (*s) {
	if (*s == '\n') {
	    *s = '\0';
	}
	s++;
    }

    argv[0] = "xargs";
    argv[1] = "-0";
    argv[2] = "ldd";
    argv[3] = NULL;
    readBuff = getOutputFrom(dir, argv, writePtr, writeBytes);
    if (!readBuff) {
	error(RPMERR_EXEC, "Failed to find requires");
	exit(1);
    }

    s = getStringBuf(readBuff);
    f = fsave = splitString(s, strlen(s), '\n');
    freeStringBuf(readBuff);
    while (*f) {
	s = *f;
	if (isspace(*s) && strstr(s, "=>")) {
	    while (isspace(*s)) {
		s++;
	    }
	    tok = s;
	    while (! isspace(*s)) {
		s++;
	    }
	    *s = '\0';
	    if ((s = strrchr(tok, '/'))) {
		tok = s + 1;
	    }
	    addReqProv(p, REQUIRE_ANY, tok, NULL);
	}

	f++;
    }
    free(fsave);

    /*** Clean Up ***/

    freeStringBuf(writeBuff);

    return 0;
}

/*************************************************************/
/*                                                           */
/* Generate and add header entry from package record         */
/*                                                           */
/*************************************************************/

int processReqProv(Header h, struct PackageRec *p)
{
    struct ReqProv *rd;
    char **nameArray, **namePtr;
    char **versionArray, **versionPtr;
    int_32 *flagArray, *flagPtr;
    

    if (p->numProv) {
	rd = p->reqprov;
	nameArray = namePtr = malloc(p->numProv * sizeof(*nameArray));
	message(MESS_VERBOSE, "Provides (%d):", p->numProv);
	while (rd) {
	    if (rd->flags & REQUIRE_PROVIDES) {
		message(MESS_VERBOSE, " %s", rd->name);
		*namePtr++ = rd->name;
	    }
	    rd = rd->next;
	}
	message(MESS_VERBOSE, "\n");

	addEntry(h, RPMTAG_PROVIDES, STRING_ARRAY_TYPE, nameArray, p->numProv);
	free(nameArray);
    }

    if (p->numConflict) {
	rd = p->reqprov;
	nameArray = namePtr = malloc(p->numConflict * sizeof(*nameArray));
	versionArray = versionPtr =
	    malloc(p->numConflict * sizeof(*versionArray));
	flagArray = flagPtr = malloc(p->numConflict * sizeof(*flagArray));
	message(MESS_VERBOSE, "Conflicts (%d):", p->numConflict);
	while (rd) {
	    if (rd->flags & REQUIRE_CONFLICTS) {
		message(MESS_VERBOSE, " %s", rd->name);
		*namePtr++ = rd->name;
		*versionPtr++ = rd->version ? rd->version : "";
		*flagPtr++ = rd->flags & REQUIRE_SENSEMASK;
	    }
	    rd = rd->next;
	}
	message(MESS_VERBOSE, "\n");

	addEntry(h, RPMTAG_CONFLICTNAME, STRING_ARRAY_TYPE,
		 nameArray, p->numConflict);
	addEntry(h, RPMTAG_CONFLICTVERSION, STRING_ARRAY_TYPE,
		 versionArray, p->numConflict);
	addEntry(h, RPMTAG_CONFLICTFLAGS, INT32_TYPE,
		 flagArray, p->numConflict);

	free(nameArray);
	free(versionArray);
	free(flagArray);
    }

    if (p->numReq) {
	rd = p->reqprov;
	nameArray = namePtr = malloc(p->numReq * sizeof(*nameArray));
	versionArray = versionPtr = malloc(p->numReq * sizeof(*versionArray));
	flagArray = flagPtr = malloc(p->numReq * sizeof(*flagArray));
	message(MESS_VERBOSE, "Requires (%d):", p->numReq);
	while (rd) {
	    if (! ((rd->flags & REQUIRE_PROVIDES) ||
		   (rd->flags & REQUIRE_CONFLICTS))) {
		message(MESS_VERBOSE, " %s", rd->name);
		*namePtr++ = rd->name;
		*versionPtr++ = rd->version ? rd->version : "";
		*flagPtr++ = rd->flags & REQUIRE_SENSEMASK;
	    }
	    rd = rd->next;
	}
	message(MESS_VERBOSE, "\n");

	addEntry(h, RPMTAG_REQUIRENAME, STRING_ARRAY_TYPE,
		 nameArray, p->numReq);
	addEntry(h, RPMTAG_REQUIREVERSION, STRING_ARRAY_TYPE,
		 versionArray, p->numReq);
	addEntry(h, RPMTAG_REQUIREFLAGS, INT32_TYPE,
		 flagArray, p->numReq);

	free(nameArray);
	free(versionArray);
	free(flagArray);
    }

    return 0;
}
