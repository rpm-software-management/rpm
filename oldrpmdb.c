#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpmerr.h"
#include "rpm_malloc.h"
#include "messages.h"
#include "misc.h"
#include "oldrpmdb.h"

static int labelstrlistToLabelList(char * str, int length, 
				   struct rpmdbLabel ** list);
static char * prefix = "/var/lib/rpm";

char * rpmdbLabelToLabelstr(struct rpmdbLabel label, int withFileNum) {
    char * c;
    char buffer[50];
 
    if (withFileNum && label.fileNumber > -1) 
	c = malloc(strlen(label.name) + strlen(label.version) + 
		   strlen(label.release) + 10);
    else
	c = malloc(strlen(label.name) + strlen(label.version) + 
		   strlen(label.release) + 3);

    strcpy(c, label.name);
    strcat(c, ":");
    strcat(c, label.version);
    strcat(c, ":");
    strcat(c, label.release);

    if (withFileNum && label.fileNumber > -1)  {
	sprintf(buffer, "%d", label.fileNumber);
	strcat(c, ":");
	strcat(c, buffer);
    }

    return c;
}

int rpmdbLabelstrToLabel(char * str, int length, struct rpmdbLabel * label) {
    char * chptr;

    label->freeType = RPMDB_FREENAME;
    label->next = NULL;
    label->name = malloc(length + 1);
    if (!label->name) {
	return 1;
    }
    memcpy(label->name, str, length);
    label->name[length] = '\0';

    chptr = label->name;
    while (*chptr != ':') chptr++;
    *chptr = '\0';
    label->version = ++chptr;
    while (*chptr != ':') chptr++;
    *chptr = '\0';
    label->release = chptr + 1;

    label->fileNumber = -1;

    /* there might be a path number tagged on to the end of this */
    while ((chptr - label->name) < length && *chptr != ':') chptr++;
    if ((chptr - label->name) < length) {
	*chptr = '\0';
	label->fileNumber = atoi(chptr + 1);
    }

    return 0;
}

static int labelstrlistToLabelList(char * str, int length, 
				   struct rpmdbLabel ** list) {
    char * start, * chptr;
    struct rpmdbLabel * head = NULL;
    struct rpmdbLabel * tail = NULL;
    struct rpmdbLabel * label;
    
    start = str;
    for (chptr = start; (chptr - str) < length; chptr++) {
	/* spaces following a space get ignored */
	if (*chptr == ' ' && start < chptr) {
	    label = malloc(sizeof(struct rpmdbLabel));
	    if (!label) {
		rpmdbFreeLabelList(head);
		return 1;
	    }
	    if (rpmdbLabelstrToLabel(start, chptr - start, label)) {
		free(label);
		rpmdbFreeLabelList(head);
		return 1;
	    }

	    if (!head) {
		head = label;
		tail = label;
	    } else {
		tail->next = label;
		tail = tail->next;
	    }

	    start = chptr + 1;
	}
    }

    /* a space on the end would break things horribly w/o this test */
    if (start < chptr) {
	label = malloc(sizeof(struct rpmdbLabel));
	if (!label) {
	    rpmdbFreeLabelList(head);
	    return 1;
	}
	if (rpmdbLabelstrToLabel(start, chptr - start, label)) {
	    free(label);
	    rpmdbFreeLabelList(head);
	    return 1;
	}

	if (!head) {
	    head = label;
	    tail = label;
	} else {
	    tail->next = label;
	    tail = tail->next;
	}

	start = chptr + 1;
    }

    *list = head;
    return 0;
}

/* returns 0 on success, -1 on failure */
int rpmdbOpen(struct rpmdb * rpmdb) {
    unsigned int gdbmFlags;
    char path[255];
    int goterr = 0;

    rpmdb->rpmdbError = RPMDB_NONE;

    gdbmFlags = GDBM_READER;

    strcpy(path, prefix);
    strcat(path, "/packages");
    rpmdb->packages = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->packages) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/nameidx");
    rpmdb->nameIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->packages) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/pathidx");
    rpmdb->pathIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->packages) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/iconidx");
    rpmdb->iconIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->iconIndex) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/groupindex");
    rpmdb->groupIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->packages) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/postidx");
    rpmdb->postIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!rpmdb->postIndex) {
	error(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    if (goterr) {
	rpmdbClose(rpmdb);
	return -1;
    }

    return 0;
}

void rpmdbClose(struct rpmdb * rpmdb) {
    gdbm_close(rpmdb->packages);
    gdbm_close(rpmdb->nameIndex);
    gdbm_close(rpmdb->pathIndex);
    gdbm_close(rpmdb->postIndex);
    gdbm_close(rpmdb->groupIndex);
    gdbm_close(rpmdb->iconIndex);
}

struct rpmdbLabel * rpmdbGetAllLabels(struct rpmdb * rpmdb) {
    datum rec;

    struct rpmdbLabel * head = NULL;
    struct rpmdbLabel * tail = NULL;
    struct rpmdbLabel * label;

    rpmdb->rpmdbError = RPMDB_NONE;

    rec = gdbm_firstkey(rpmdb->packages);
    while (rec.dptr) {
	label = malloc(sizeof(struct rpmdbLabel));
	if (!label) {
	    rpmdbFreeLabelList(head);
	    rpmdb->rpmdbError = RPMDB_NO_MEMORY;
	    return NULL;
	}
	if (rpmdbLabelstrToLabel(rec.dptr, rec.dsize, label)) {
	    free(label);
	    rpmdbFreeLabelList(head);
	    rpmdb->rpmdbError = RPMDB_NO_MEMORY;
	    return NULL;
	}

	if (!head) {
	    head = label;
	    tail = label;
	} else {
	    tail->next = label;
	    tail = tail->next;
	}

	rec = gdbm_nextkey(rpmdb->packages, rec);
    }

    return head;
}

struct rpmdbLabel * rpmdbFindPackagesByFile(struct rpmdb * rpmdb, char * path) {
    datum rec;
    datum key;
    struct rpmdbLabel * list;

    rpmdb->rpmdbError = RPMDB_NONE;

    key.dptr = path;
    key.dsize = strlen(path);
    rec = gdbm_fetch(rpmdb->pathIndex, key);
    
    if (!rec.dptr) 
	return NULL;
    if (labelstrlistToLabelList(rec.dptr, rec.dsize, &list)) {
	free(rec.dptr);
	rpmdb->rpmdbError = RPMDB_NO_MEMORY;
	return NULL;
    }
    free(rec.dptr);

    return list;
}

struct rpmdbLabel * rpmdbFindPackagesByLabel(struct rpmdb * rpmdb, 
					     struct rpmdbLabel label)

/* the Name has to be here. The version/release fields optionally
   restrict the search. Either will do. */

{
    datum rec;
    datum key;
    struct rpmdbLabel * list;
    struct rpmdbLabel * prospect;
    struct rpmdbLabel * parent;
    int bad;

    rpmdb->rpmdbError = RPMDB_NONE;

    key.dptr = label.name;
    key.dsize = strlen(label.name);
    rec = gdbm_fetch(rpmdb->nameIndex, key);
    
    if (!rec.dptr) 
	return NULL;
    if (labelstrlistToLabelList(rec.dptr, rec.dsize, &list)) {
	free(rec.dptr);
	rpmdb->rpmdbError = RPMDB_NO_MEMORY;
	return NULL;
    }
    free(rec.dptr);

    prospect = list;
    parent = NULL;
    while (prospect) {
	bad = 0;
	if (label.version && strcmp(label.version, prospect->version))
	    bad = 1;
	else if (label.release && strcmp(label.release, prospect->release))
	    bad = 1;

	if (bad) {
	    rpmdbFreeLabel(*prospect);
	    if (!parent) {
		list = prospect->next;
		free(prospect);
		prospect = list;
	    } else {
		parent->next = prospect->next;
		free(prospect);
		prospect = parent->next;
	    }
	} else {
	    prospect = prospect->next;
	}
    }

    return list;
}

struct rpmdbLabel rpmdbMakeLabel(char * name, char * version, char * release,
				 int fileNumber, enum rpmdbFreeType freeType) {
    struct rpmdbLabel label;

    label.next = NULL;
    label.freeType = freeType;
    label.name = name;
    label.version = version;
    label.release = release;
    label.fileNumber = fileNumber;

    return label;
}

void rpmdbFreeLabelList(struct rpmdbLabel * list) {
    struct rpmdbLabel * saved;

    while (list) {
	rpmdbFreeLabel(*list);
	saved = list->next;
	free(list);
	list = saved;
    }
}

void rpmdbFreeLabel(struct rpmdbLabel label) {
    if (label.freeType == RPMDB_NOFREE) return;

    free(label.name);
    if (label.freeType == RPMDB_FREEALL) {
	free(label.version);
	free(label.release);
    }
}

/* Returns NULL on error */
char * rpmdbGetPackageGroup(struct rpmdb * rpmdb, struct rpmdbLabel label) {
    datum key, rec;
    char * g;
    
    key.dptr = label.name;
    key.dsize = strlen(label.name);
    
    rec = gdbm_fetch(rpmdb->groupIndex, key);
    if (!rec.dptr) {
	return strdup("Unknown");
    }
    
    g = malloc(rec.dsize + 1);
    strncpy(g, rec.dptr, rec.dsize);
    g[rec.dsize] = '\0';
    free(rec.dptr);

    return g;
}

/* Returns NULL on error or if no icon exists */
char * rpmdbGetPackageGif(struct rpmdb * rpmdb, struct rpmdbLabel label,
			  int * size) {
    datum key, rec;
    char * labelstr;

    labelstr = rpmdbLabelToLabelstr(label, 0);
    
    key.dptr = labelstr;
    key.dsize = strlen(labelstr);
    
    rec = gdbm_fetch(rpmdb->iconIndex, key);
    free(labelstr);
    if (!rec.dptr) {
	return NULL;
    }

    *size = rec.dsize;

    return rec.dptr;
}

/* return 0 on success, 1 on failure */
int rpmdbGetPackageInfo(struct rpmdb * rpmdb, struct rpmdbLabel label,
			struct rpmdbPackageInfo * pinfo) {
    char * labelstr;
    char ** list, ** prelist;
    char ** strptr;
    datum key, rec;
    int i, j;

    labelstr = rpmdbLabelToLabelstr(label, 0);

    message(MESS_DEBUG, "pulling %s from database\n", labelstr);

    key.dptr = labelstr;
    key.dsize = strlen(labelstr);
    
    rec = gdbm_fetch(rpmdb->packages, key);
    if (!rec.dptr) {
	error(RPMERR_OLDDBCORRUPT, "package not found in database");
	return 1;
    }

    free(labelstr);

    list = splitString(rec.dptr, rec.dsize, '\1');
    free(rec.dptr);

    pinfo->version = strdup(list[1]); 
    pinfo->release = strdup(list[2]); 
    /* list[3] == "1" always */
    pinfo->name = malloc(strlen(list[0]) + strlen(list[4]) + 2);
    strcpy(pinfo->name, list[0]);
    if (strlen(list[4])) {
	strcat(pinfo->name, "-");
	strcat(pinfo->name, list[4]);
    }
    pinfo->labelstr = malloc(strlen(pinfo->name) + strlen(pinfo->version) +
			     strlen(pinfo->release) + 3);
    strcpy(pinfo->labelstr, pinfo->name);
    strcat(pinfo->labelstr, ":");
    strcat(pinfo->labelstr, pinfo->version);
    strcat(pinfo->labelstr, ":");
    strcat(pinfo->labelstr, pinfo->release);

    pinfo->preamble = strdup(list[5]);
    pinfo->installTime = atoi(list[6]);
    pinfo->fileCount = atoi(list[7]);
    
    prelist = splitString(pinfo->preamble, strlen(pinfo->preamble), '\n');

    /* these are optional */
    pinfo->distribution = NULL;
    pinfo->vendor = NULL;
    pinfo->description = NULL;
    pinfo->copyright = NULL;

    for (strptr = prelist; *strptr; strptr++) {
	if (!strncasecmp("Description: ", *strptr, 13))
	    pinfo->description = strdup((*strptr) + 13);
	else if (!strncasecmp("Copyright: ", *strptr, 11))
	    pinfo->copyright = strdup((*strptr) + 11);
	else if (!strncasecmp("Distribution: ", *strptr, 14))
	    pinfo->distribution = strdup((*strptr) + 14);
	else if (!strncasecmp("Vendor: ", *strptr, 8))
	    pinfo->vendor = strdup((*strptr) + 8);
	else if (!strncasecmp("size: ", *strptr, 6))
	    pinfo->size = atoi((*strptr) + 6);
	else if (!strncasecmp("BuildDate: ", *strptr, 11))
	    pinfo->buildTime =atoi((*strptr) + 11);
	else if (!strncasecmp("BuildHost: ", *strptr, 11))
	    pinfo->buildHost = strdup((*strptr) + 11);
    }
    freeSplitString(prelist);

    if (!pinfo->vendor) pinfo->vendor = strdup("");
    if (!pinfo->description) pinfo->description = strdup("");
    if (!pinfo->distribution) pinfo->distribution = strdup("");
    if (!pinfo->copyright) {
	pinfo->copyright = strdup("");
	printf("no copyright!\n");
    }

    pinfo->files = malloc(sizeof(struct rpmFileInfo) * pinfo->fileCount);

    j = 8;
    for (i = 0; i < pinfo->fileCount; i++) {
	rpmfileFromInfoLine(list[j], list[j + 1], list[j + 2],
			    &pinfo->files[i]);
	j += 3;
    }

    freeSplitString(list);
		
    return 0;
}

void rpmdbFreePackageInfo(struct rpmdbPackageInfo package) {
    int i;

    free(package.version);
    free(package.release);
    free(package.name);
    free(package.labelstr);
    free(package.buildHost);
    free(package.vendor);
    free(package.description);
    free(package.copyright);
    free(package.distribution);
    free(package.preamble);

    for (i = 0; i < package.fileCount; i++) {
	rpmfileFree(&package.files[i]);
    }

    free(package.files);
}

int rpmdbLabelCmp(struct rpmdbLabel * one, struct rpmdbLabel * two) {
    int i;

    if ((i = strcmp(one->name, two->name)))
	return i;
    else if ((i = strcmp(one->version, two->version)))
	return i;
    else
	return strcmp(one->release, two->release);
}

void rpmdbSetPrefix(char * new) {
    prefix = new;
}
