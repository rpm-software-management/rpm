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

static void parseFileForProv(char *f, struct PackageRec *p);

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

static void parseFileForProv(char *f, struct PackageRec *p)
{
    char soname[1024];
    char command[2048];
    char *s;
    FILE *pipe;
    int len;

    s = f + strlen(f) - 1;
    while (*s != '/') {
	s--;
    }
    s++;
    
    if (strstr(s, ".so")) {
	sprintf(command,
		"objdump --raw %s%s --section=.dynstr 2> /dev/null |"
		"tr '\\0' '\\n' | tail -1",
		getVar(RPMVAR_ROOT) ? getVar(RPMVAR_ROOT) : "", f);
	pipe = popen(command, "r");
	soname[0] = '\0';
	fgets(soname, sizeof(soname)-1, pipe);
	pclose(pipe);
	/* length 1 lines are empty */
	if ((len = strlen(soname)) > 1) {
	    soname[len-1] = '\0';
	    if (strcmp(soname, "_end")) {
		addReqProv(p, REQUIRE_PROVIDES, soname, NULL);
	    } else {
		/* _end means no embedded soname */
		addReqProv(p, REQUIRE_PROVIDES, s, NULL);
	    }
	}
    }
}

int generateAutoReqProv(Header header, struct PackageRec *p)
{
    char **f, **fsave, *s, *tok;
    int count;
    int lddPID;
    int lddDead;
    int toLdd[2];
    int fromLdd[2];
    int_16 *modes;

    StringBuf writeBuff;
    StringBuf readBuff;
    char *writePtr;
    int writeBytesLeft, bytesWritten;

    int bytes;
    unsigned char buf[8193];

    int status;
    void *oldhandler;

    message(MESS_VERBOSE, "Finding dependencies...\n");

    pipe(toLdd);
    pipe(fromLdd);
    
    oldhandler = signal(SIGPIPE, SIG_IGN);

    if (!(lddPID = fork())) {
	close(0);
	close(1);
	close(toLdd[1]);
	close(fromLdd[0]);
	
	dup2(toLdd[0], 0);   /* Make stdin the in pipe */
	dup2(fromLdd[1], 1); /* Make stdout the out pipe */
	close(2);            /* Toss stderr */

	if (getVar(RPMVAR_ROOT)) {
	    if (chdir(getVar(RPMVAR_ROOT))) {
		error(RPMERR_EXEC, "Couldn't chdir to %s",
		      getVar(RPMVAR_ROOT));
		exit(RPMERR_EXEC);
	    }
	} else {
	    chdir("/");
	}

	execlp("xargs", "xargs", "-0", "ldd", NULL);
	error(RPMERR_EXEC, "Couldn't exec ldd");
	exit(RPMERR_EXEC);
    }
    if (lddPID < 0) {
	error(RPMERR_FORK, "Couldn't fork ldd (xargs)");
	return RPMERR_FORK;
    }

    close(toLdd[0]);
    close(fromLdd[1]);

    /* Do not block reading or writing from/to ldd. */
    fcntl(fromLdd[0], F_SETFL, O_NONBLOCK);
    fcntl(toLdd[1], F_SETFL, O_NONBLOCK);

    if (!getEntry(header, RPMTAG_FILENAMES, NULL, (void **) &f, &count)) {
	/* count may already be 0, but this is safer */
	count = 0;
	fsave = NULL;
    } else {
	fsave = f;
	getEntry(header, RPMTAG_FILEMODES, NULL, (void **) &modes, NULL);
    }

    readBuff = newStringBuf();
    
    writeBuff = newStringBuf();
    writeBytesLeft = 0;
    while (count--) {
        s = *f++;
	if (S_ISREG(*modes++)) {
	    /* We skip the leading "/" (already normalized) */
	    writeBytesLeft += strlen(s);
	    appendLineStringBuf(writeBuff, s + 1);
	    parseFileForProv(s, p);
	}
    }
    if (fsave) {
	free(fsave);
    }
    writePtr = getStringBuf(writeBuff);
    s = writePtr;
    while (*s) {
	if (*s == '\n') {
	    *s = '\0';
	}
	s++;
    }
   
    lddDead = 0;
    do {
	if (waitpid(lddPID, &status, WNOHANG)) {
	    lddDead = 1;
	}

	/* Write some stuff to the ldd process if possible */
        if (writeBytesLeft) {
	    if ((bytesWritten =
		  write(toLdd[1], writePtr,
		    (1024<writeBytesLeft) ? 1024 : writeBytesLeft)) < 0) {
	        if (errno != EAGAIN) {
		    perror("generateAutoReqProv");
	            exit(1);
		}
	        bytesWritten = 0;
	    }
	    writeBytesLeft -= bytesWritten;
	    writePtr += bytesWritten;
	} else {
	    close(toLdd[1]);
	}
	
	/* Read any data from ldd */
	bytes = read(fromLdd[0], buf, sizeof(buf)-1);
	while (bytes > 0) {
	    buf[bytes] = '\0';
	    appendStringBuf(readBuff, buf);
	    bytes = read(fromLdd[0], buf, sizeof(buf)-1);
	}

	/* terminate when ldd dies */
    } while (!lddDead);

    close(toLdd[1]);
    close(fromLdd[0]);
    
    signal(SIGPIPE, oldhandler);

    if (writeBytesLeft) {
	error(RPMERR_EXEC, "failed to write all data to ldd");
	return 1;
    }
    waitpid(lddPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_EXEC, "ldd failed");
	return 1;
    }

    freeStringBuf(writeBuff);
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
