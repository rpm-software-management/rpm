#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#include "rpmerr.h"
#include "rpmlib.h"

int rpmRemovePackage(char * prefix, rpmdb db, unsigned int offset, int test) {
    Header h;
    int i;
    int count;
    char * rmmess;
    char * fnbuffer = NULL;
    int fnbuffersize = 0;
    int prefixLength = strlen(prefix);
    char ** fileList;
    int type;
    char * fileStatesList;

    h = rpmdbGetRecord(db, offset);
    if (!h) {
	error(RPMERR_DBCORRUPT, "cannot read header at %d for uninstall",
	      offset);
	return 0;
    }

    /* dependency checking should go in here */

    if (test) {
	rmmess = "would remove";
    } else {
	rmmess = "removing";
    }
    
    message(MESS_DEBUG, "%s files test = %d\n", rmmess, test);
    if (!getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
	 &count)) {
	puts("(contains no files)");
    } else {
	if (prefix[0]) {
	    fnbuffersize = 1024;
	    fnbuffer = alloca(fnbuffersize);
	}

	getEntry(h, RPMTAG_FILESTATES, &type, 
		 (void **) &fileStatesList, &count);

	for (i = 0; i < count; i++) {
	    if (prefix[0]) {
		if ((strlen(fileList[i]) + prefixLength + 1) > fnbuffersize) {
		    fnbuffersize = (strlen(fileList[i]) + prefixLength) * 2;
		    fnbuffer = alloca(fnbuffersize);
		}
		strcpy(fnbuffer, "");
		strcat(fnbuffer, "/");
		strcpy(fnbuffer, fileList[i]);
	    } else {
		fnbuffer = fileList[i];
	    }
	
	    switch (fileStatesList[i]) { 
	      case RPMFILE_STATE_REPLACED:
		message(MESS_DEBUG, "%s has already been replaced\n", 
			fnbuffer);
		break;

	      case RPMFILE_STATE_NORMAL:
		message(MESS_DEBUG, "%s - %s\n", fnbuffer, rmmess);
		/* unlink(fnbuffer); */
		break;
	    }
	}

	free(fileList);
    }

    message(MESS_DEBUG, "%s database entry\n", rmmess);
    if (!test)
	rpmdbRemove(db, offset, 0);

    return 1;
}

