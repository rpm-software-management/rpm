#include "system.h"

#include <rpmbuild.h>

#include "rpm_malloc.h"
#include "oldrpmdb.h"
#include "debug.h"

static int labelstrlistToLabelList(char * str, int length, 
				   struct oldrpmdbLabel ** list);
static char * getScript(char * which, struct oldrpmdb *oldrpmdb, 
		        struct oldrpmdbLabel label);

static char * prefix = "/var/lib/rpm";

char * oldrpmdbLabelToLabelstr(struct oldrpmdbLabel label, int withFileNum) {
    char * c;
    char buffer[50];
 
    if (withFileNum && label.fileNumber > -1) 
	c = xmalloc(strlen(label.name) + strlen(label.version) + 
		   strlen(label.release) + 10);
    else
	c = xmalloc(strlen(label.name) + strlen(label.version) + 
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

int oldrpmdbLabelstrToLabel(char * str, int length, struct oldrpmdbLabel * label) {
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
				   struct oldrpmdbLabel ** list) {
    char * start, * chptr;
    struct oldrpmdbLabel * head = NULL;
    struct oldrpmdbLabel * tail = NULL;
    struct oldrpmdbLabel * label;
    
    start = str;
    for (chptr = start; (chptr - str) < length; chptr++) {
	/* spaces following a space get ignored */
	if (*chptr == ' ' && start < chptr) {
	    label = malloc(sizeof(struct oldrpmdbLabel));
	    if (!label) {
		oldrpmdbFreeLabelList(head);
		return 1;
	    }
	    if (oldrpmdbLabelstrToLabel(start, chptr - start, label)) {
		free(label);
		oldrpmdbFreeLabelList(head);
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
	label = malloc(sizeof(struct oldrpmdbLabel));
	if (!label) {
	    oldrpmdbFreeLabelList(head);
	    return 1;
	}
	if (oldrpmdbLabelstrToLabel(start, chptr - start, label)) {
	    free(label);
	    oldrpmdbFreeLabelList(head);
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
int oldrpmdbOpen(struct oldrpmdb * oldrpmdb) {
    unsigned int gdbmFlags;
    char path[255];
    int goterr = 0;

    oldrpmdb->rpmdbError = RPMDB_NONE;

    gdbmFlags = GDBM_READER;

    strcpy(path, prefix);
    strcat(path, "/packages");
    oldrpmdb->packages = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->packages) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/nameidx");
    oldrpmdb->nameIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->packages) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/pathidx");
    oldrpmdb->pathIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->packages) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/iconidx");
    oldrpmdb->iconIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->iconIndex) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/groupindex");
    oldrpmdb->groupIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->packages) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    strcpy(path, prefix);
    strcat(path, "/postidx");
    oldrpmdb->postIndex = gdbm_open(path, 1024, gdbmFlags, 0644, NULL);
    if (!oldrpmdb->postIndex) {
	rpmError(RPMERR_GDBMOPEN, path, gdbm_strerror(gdbm_errno));
	goterr = 1;
    }

    if (goterr) {
	oldrpmdbClose(oldrpmdb);
	return -1;
    }

    return 0;
}

void oldrpmdbClose(struct oldrpmdb * oldrpmdb) {
    gdbm_close(oldrpmdb->packages);
    gdbm_close(oldrpmdb->nameIndex);
    gdbm_close(oldrpmdb->pathIndex);
    gdbm_close(oldrpmdb->postIndex);
    gdbm_close(oldrpmdb->groupIndex);
    gdbm_close(oldrpmdb->iconIndex);
}

struct oldrpmdbLabel * oldrpmdbGetAllLabels(struct oldrpmdb * oldrpmdb) {
    datum rec;

    struct oldrpmdbLabel * head = NULL;
    struct oldrpmdbLabel * tail = NULL;
    struct oldrpmdbLabel * label;

    oldrpmdb->rpmdbError = RPMDB_NONE;

    rec = gdbm_firstkey(oldrpmdb->packages);
    while (rec.dptr) {
	label = malloc(sizeof(struct oldrpmdbLabel));
	if (!label) {
	    oldrpmdbFreeLabelList(head);
	    oldrpmdb->rpmdbError = RPMDB_NO_MEMORY;
	    return NULL;
	}
	if (oldrpmdbLabelstrToLabel(rec.dptr, rec.dsize, label)) {
	    free(label);
	    oldrpmdbFreeLabelList(head);
	    oldrpmdb->rpmdbError = RPMDB_NO_MEMORY;
	    return NULL;
	}

	if (!head) {
	    head = label;
	    tail = label;
	} else {
	    tail->next = label;
	    tail = tail->next;
	}

	rec = gdbm_nextkey(oldrpmdb->packages, rec);
    }

    return head;
}

struct oldrpmdbLabel * oldrpmdbFindPackagesByFile(struct oldrpmdb * oldrpmdb, char * path) {
    datum rec;
    datum key;
    struct oldrpmdbLabel * list;

    oldrpmdb->rpmdbError = RPMDB_NONE;

    key.dptr = path;
    key.dsize = strlen(path);
    rec = gdbm_fetch(oldrpmdb->pathIndex, key);
    
    if (!rec.dptr) 
	return NULL;
    if (labelstrlistToLabelList(rec.dptr, rec.dsize, &list)) {
	free(rec.dptr);
	oldrpmdb->rpmdbError = RPMDB_NO_MEMORY;
	return NULL;
    }
    free(rec.dptr);

    return list;
}

struct oldrpmdbLabel * oldrpmdbFindPackagesByLabel(struct oldrpmdb * oldrpmdb, 
					     struct oldrpmdbLabel label)

/* the Name has to be here. The version/release fields optionally
   restrict the search. Either will do. */

{
    datum rec;
    datum key;
    struct oldrpmdbLabel * list;
    struct oldrpmdbLabel * prospect;
    struct oldrpmdbLabel * parent;
    int bad;

    oldrpmdb->rpmdbError = RPMDB_NONE;

    key.dptr = label.name;
    key.dsize = strlen(label.name);
    rec = gdbm_fetch(oldrpmdb->nameIndex, key);
    
    if (!rec.dptr) 
	return NULL;
    if (labelstrlistToLabelList(rec.dptr, rec.dsize, &list)) {
	free(rec.dptr);
	oldrpmdb->rpmdbError = RPMDB_NO_MEMORY;
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
	    oldrpmdbFreeLabel(*prospect);
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

struct oldrpmdbLabel oldrpmdbMakeLabel(char * name, char * version, char * release,
				 int fileNumber, enum oldrpmdbFreeType freeType) {
    struct oldrpmdbLabel label;

    label.next = NULL;
    label.freeType = freeType;
    label.name = name;
    label.version = version;
    label.release = release;
    label.fileNumber = fileNumber;

    return label;
}

void oldrpmdbFreeLabelList(struct oldrpmdbLabel * list) {
    struct oldrpmdbLabel * saved;

    while (list) {
	oldrpmdbFreeLabel(*list);
	saved = list->next;
	free(list);
	list = saved;
    }
}

void oldrpmdbFreeLabel(struct oldrpmdbLabel label) {
    if (label.freeType == RPMDB_NOFREE) return;

    free(label.name);
    if (label.freeType == RPMDB_FREEALL) {
	free(label.version);
	free(label.release);
    }
}

/* Returns NULL on error */
char * oldrpmdbGetPackageGroup(struct oldrpmdb * oldrpmdb, struct oldrpmdbLabel label) {
    datum key, rec;
    char * g;
    
    key.dptr = label.name;
    key.dsize = strlen(label.name);
    
    rec = gdbm_fetch(oldrpmdb->groupIndex, key);
    if (!rec.dptr) {
	return xstrdup("Unknown");
    }
    
    g = malloc(rec.dsize + 1);
    strncpy(g, rec.dptr, rec.dsize);
    g[rec.dsize] = '\0';
    free(rec.dptr);

    return g;
}

static char * getScript(char * which, struct oldrpmdb *oldrpmdb, 
		        struct oldrpmdbLabel label) {
    datum key, rec;
    char * labelstr, * l;

    labelstr = oldrpmdbLabelToLabelstr(label, 0);
    labelstr = xrealloc(labelstr, strlen(labelstr) + 10);
    strcat(labelstr, ":");
    strcat(labelstr, which);

    key.dptr = labelstr;
    key.dsize = strlen(labelstr);
    
    rec = gdbm_fetch(oldrpmdb->postIndex, key);
    free(labelstr);

    if (!rec.dptr) return NULL;

    l = malloc(rec.dsize + 1);
    strncpy(l, rec.dptr, rec.dsize);
    l[rec.dsize] = '\0';

    free(rec.dptr);

    return l;
}

char *oldrpmdbGetPackagePostun (struct oldrpmdb *oldrpmdb, 
				struct oldrpmdbLabel label) {
    return getScript("post", oldrpmdb, label);
}

char *oldrpmdbGetPackagePreun (struct oldrpmdb *oldrpmdb, 
				struct oldrpmdbLabel label) {
    return getScript("pre", oldrpmdb, label);
}

/* Returns NULL on error or if no icon exists */
char * oldrpmdbGetPackageGif(struct oldrpmdb * oldrpmdb, 
			     struct oldrpmdbLabel label, int * size) {
    datum key, rec;
    char * labelstr;

    labelstr = oldrpmdbLabelToLabelstr(label, 0);
    
    key.dptr = labelstr;
    key.dsize = strlen(labelstr);
    
    rec = gdbm_fetch(oldrpmdb->iconIndex, key);
    free(labelstr);
    if (!rec.dptr) {
	return NULL;
    }

    *size = rec.dsize;

    return rec.dptr;
}

/* return 0 on success, 1 on failure */
int oldrpmdbGetPackageInfo(struct oldrpmdb * oldrpmdb, struct oldrpmdbLabel label,
			struct oldrpmdbPackageInfo * pinfo) {
    char * labelstr;
    char ** list, ** prelist;
    char ** strptr;
    datum key, rec;
    int i, j;

    labelstr = oldrpmdbLabelToLabelstr(label, 0);

    rpmMessage(RPMMESS_DEBUG, _("pulling %s from database\n"), labelstr);

    key.dptr = labelstr;
    key.dsize = strlen(labelstr);
    
    rec = gdbm_fetch(oldrpmdb->packages, key);
    if (!rec.dptr) {
	rpmError(RPMERR_OLDDBCORRUPT, _("package not found in database\n"));
	return 1;
    }

    free(labelstr);

    list = splitString(rec.dptr, rec.dsize, '\1');
    free(rec.dptr);

    pinfo->version = xstrdup(list[1]); 
    pinfo->release = xstrdup(list[2]); 
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

    pinfo->preamble = xstrdup(list[5]);
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
	    pinfo->description = xstrdup((*strptr) + 13);
	else if (!strncasecmp("Copyright: ", *strptr, 11))
	    pinfo->copyright = xstrdup((*strptr) + 11);
	else if (!strncasecmp("Distribution: ", *strptr, 14))
	    pinfo->distribution = xstrdup((*strptr) + 14);
	else if (!strncasecmp("Vendor: ", *strptr, 8))
	    pinfo->vendor = xstrdup((*strptr) + 8);
	else if (!strncasecmp("size: ", *strptr, 6))
	    pinfo->size = atoi((*strptr) + 6);
	else if (!strncasecmp("BuildDate: ", *strptr, 11))
	    pinfo->buildTime =atoi((*strptr) + 11);
	else if (!strncasecmp("BuildHost: ", *strptr, 11))
	    pinfo->buildHost = xstrdup((*strptr) + 11);
    }
    freeSplitString(prelist);

    if (!pinfo->vendor) pinfo->vendor = xstrdup("");
    if (!pinfo->description) pinfo->description = xstrdup("");
    if (!pinfo->distribution) pinfo->distribution = xstrdup("");
    if (!pinfo->copyright) {
	pinfo->copyright = xstrdup("");
	fprintf(stdout, _("no copyright!\n"));
    }

    pinfo->files = malloc(sizeof(struct oldrpmFileInfo) * pinfo->fileCount);

    j = 8;
    for (i = 0; i < pinfo->fileCount; i++) {
	oldrpmfileFromInfoLine(list[j], list[j + 1], list[j + 2],
			    &pinfo->files[i]);
	j += 3;
    }

    freeSplitString(list);
		
    return 0;
}

void oldrpmdbFreePackageInfo(struct oldrpmdbPackageInfo package) {
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
	oldrpmfileFree(&package.files[i]);
    }

    free(package.files);
}

int oldrpmdbLabelCmp(struct oldrpmdbLabel * one, struct oldrpmdbLabel * two) {
    int i;

    if ((i = strcmp(one->name, two->name)))
	return i;
    else if ((i = strcmp(one->version, two->version)))
	return i;
    else
	return strcmp(one->release, two->release);
}

void oldrpmdbSetPrefix(char * new) {
    prefix = new;
}
