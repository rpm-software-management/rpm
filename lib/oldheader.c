#include "system.h"

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmlib.h>

#include "misc.h"
#include "oldheader.h"

/* This *can't* read 1.0 headers -- it needs 1.1 (w/ group and icon fields)
   or better. I'd be surprised if any 1.0 headers are left anywhere anyway.
   Red Hat 2.0 shipped with 1.1 headers, but some old BETAs used 1.0. */

struct literalHeader {
    unsigned char m1, m2, m3, m4;
    unsigned char major, minor;

    unsigned short type, cpu;
    char labelstr[66];
    unsigned int specOffset;
    unsigned int specLength;
    unsigned int archiveOffset;
    unsigned int size;
    unsigned int os;
    unsigned int groupLength;
    unsigned int iconLength;
} ;

/* this leaves the file pointer pointing at the data section */

char * oldhdrReadFromStream(FD_t fd, struct oldrpmHeader * header) {
    struct literalHeader lit;
    char * chptr;
    int bytesRead;
    char ch;
    unsigned int specOffset;
    unsigned int archiveOffset;
    unsigned int groupLength;

    if (timedRead(fd, &lit, sizeof(lit)) != sizeof(lit)) {
	return strerror(errno);
    }

    bytesRead = sizeof(lit);

    if (lit.m1 != 0xed || lit.m2 != 0xab || lit.m3 != 0xee ||
	lit.m4 != 0xdb) {
	return "bad magic for RPM package";
    }

    specOffset = htonl(lit.specOffset);
    header->specLength = htonl(lit.specLength);
    archiveOffset = htonl(lit.archiveOffset);
    header->size = htonl(lit.size);
    header->os = htonl(lit.os);
    groupLength = htonl(lit.groupLength);
    header->iconLength = htonl(lit.iconLength);

    header->spec = xmalloc(header->specLength);
    header->name = xmalloc(strlen(lit.labelstr) + 1);
    if (!header->spec || !header->name) {
	if (header->spec) free(header->spec);
	if (header->name) free(header->name);
	return "out of memory";
    }

    strcpy(header->name, lit.labelstr);
    chptr = header->name + strlen(header->name);
    while (*chptr != '-') chptr--;
    *chptr = '\0';
    header->release = chptr + 1;
    while (*chptr != '-') chptr--;
    *chptr = '\0';
    header->version = chptr + 1;

    if (groupLength) {
        header->group = xmalloc(groupLength + 1);
	if (!header->group) {
	    free(header->spec);
	    free(header->name);
	    return "out of memory";
	}

	if (timedRead(fd, header->group, groupLength) != groupLength) {
	    oldhdrFree(header);
	    return strerror(errno);
	}
	header->group[groupLength] = '\0';
	bytesRead += groupLength;
    } else {
	header->group = NULL;
    }
	
    if (header->iconLength) {
	header->icon = xmalloc(header->iconLength);
	if (!header->icon) {
	    free(header->spec);
	    free(header->name);
	    free(header->icon);
	    return "out of memory";
	}
	if (timedRead(fd, header->icon, header->iconLength) != 
			header->iconLength) {
	    oldhdrFree(header);
	    return strerror(errno);
	}
	bytesRead += header->iconLength;
    } else {
	header->icon = NULL;
    }

    while (bytesRead < specOffset) {
	if (timedRead(fd, &ch, 1) != 1) {
	    oldhdrFree(header);
	    return strerror(errno);
	}
	bytesRead++;
    }

    if (timedRead(fd, header->spec, header->specLength) != header->specLength) {
	oldhdrFree(header);
	return strerror(errno);
    }
    bytesRead += header->specLength;

    while (bytesRead < archiveOffset) {
	if (timedRead(fd, &ch, 1) != 1) {
	    oldhdrFree(header);
	    return strerror(errno);
	}
	bytesRead++;
    }

    return NULL;
}

char * oldhdrReadFromFile(char * filename, struct oldrpmHeader * header) {
    char * rc;
    FD_t fd;

    fd = fdOpen(filename, O_RDONLY, 0);
    if (fdFileno(fd) < 0) return strerror(errno);
    
    rc = oldhdrReadFromStream(fd, header);
    fdClose(fd);
    
    return rc;
}

void oldhdrFree(struct oldrpmHeader * header) {
    free(header->name);
    free(header->spec);
    if (header->icon) free(header->icon);
    if (header->group) free(header->group);
}

void oldhdrSpecFree(struct oldrpmHeaderSpec * spec) {
    free(spec->copyright);
    free(spec->description);
    free(spec->vendor);
    free(spec->distribution);
    free(spec->buildHost);
    if (spec->postun) free(spec->postun);
    if (spec->postin) free(spec->postin);
    if (spec->preun) free(spec->preun);
    if (spec->prein) free(spec->prein);

    while (spec->fileCount) {
	spec->fileCount--;
	oldrpmfileFree(spec->files + spec->fileCount);
    }

    free(spec->files);
}

char * oldhdrParseSpec(struct oldrpmHeader * header, struct oldrpmHeaderSpec * spec) {
    char ** lines;
    char ** strptr;
    char ** files = NULL;
    char ** str = NULL;
    int strlength = 0;
    int i;
    enum { FILELIST, PREIN, POSTIN, PREUN, POSTUN, PREAMBLE } state = PREAMBLE;

    lines = splitString(header->spec, header->specLength, '\n');
    if (!lines) {
	return "out of memory";
    }

    /* these are optional */
    spec->distribution = NULL;
    spec->vendor = NULL;
    spec->description = NULL;
    spec->copyright = NULL;
    spec->prein = spec->postin = NULL;
    spec->preun = spec->postun = NULL;

    spec->fileCount = 0;
    for (strptr = lines; *strptr; strptr++) {
	if (!strncmp("%speci", *strptr, sizeof("%speci")-1)) {
	    state = FILELIST;
	    files = strptr + 1;
	} else if (!strncmp("%postun", *strptr, sizeof("%postun")-1)) {
	    state = POSTUN;
	    str = &spec->postun;
	}
	else if (!strncmp("%preun", *strptr, sizeof("%preun")-1)) {
	    state = PREUN;
	    str = &spec->preun;
	}
	else if (!strncmp("%post", *strptr, sizeof("%post")-1)) {
	    state = POSTIN;
	    str = &spec->postin;
	}
	else if (!strncmp("%pre", *strptr, sizeof("%pre")-1)) {
	    state = PREIN;
	    str = &spec->prein;
	}
	else {
	    switch (state) {
	      case FILELIST: 
		if (**strptr) 
		    spec->fileCount++;
		break;

	      case PREAMBLE:
		if (!strncmp("Description: ", *strptr, 13))
		    spec->description = xstrdup((*strptr) + 13);
		else if (!strncmp("Distribution: ", *strptr, 14))
		    spec->distribution = xstrdup((*strptr) + 14);
		else if (!strncmp("Vendor: ", *strptr, 8))
		    spec->vendor = xstrdup((*strptr) + 8);
		else if (!strncmp("BuildHost: ", *strptr, 11))
		    spec->buildHost = xstrdup((*strptr) + 11);
		else if (!strncmp("BuildDate: ", *strptr, 11))
		    spec->buildTime = atoi((*strptr) + 11);
		else if (!strncmp("Copyright: ", *strptr, 11))
		    spec->copyright = xstrdup((*strptr) + 11);
		break;

	      case PREUN: case PREIN: case POSTIN: case POSTUN:
		if (!*str)  {
		    *str = xmalloc(strlen(*strptr) + 2);
		    strlength = 0;
		    (*str)[0] = '\0';
		}
		else
		    *str = xrealloc(*str, strlength + strlen(*strptr) + 2);
		strcat(*str, *strptr);
		strcat(*str, "\n");
		strlength += strlen(*strptr) + 1;
	    }
		
	}
    }

    spec->files = xmalloc(sizeof(struct oldrpmFileInfo) * spec->fileCount);
    if (!spec->files) {
	freeSplitString(lines);
	return "out of memory";
    }

    for (strptr = files, i = 0; *strptr; strptr++, i++) {
	if (**strptr) 
	    oldrpmfileFromSpecLine(*strptr, spec->files + i);
    }

    freeSplitString(lines);

    if (!spec->vendor) spec->vendor = xstrdup("");
    if (!spec->description) spec->description = xstrdup("");
    if (!spec->distribution) spec->distribution = xstrdup("");
    if (!spec->copyright) spec->copyright = xstrdup("");

    return NULL;
}

static void infoFromFields(char ** fields, struct oldrpmFileInfo * fi);

void oldrpmfileFromInfoLine(char * path, char * state, char * str, 
			struct oldrpmFileInfo * fi) {
    char ** fields;

    fields = splitString(str, strlen(str), ' ');

    fi->path = xstrdup(path);
    if (!strcmp(state, "normal"))
	fi->state = RPMFILE_STATE_NORMAL;
    else if (!strcmp(state, "replaced"))
	fi->state = RPMFILE_STATE_REPLACED;
    else 
	rpmError(RPMERR_INTERNAL, _("bad file state: %s"), state);

    infoFromFields(fields, fi);

    freeSplitString(fields);
}

void oldrpmfileFromSpecLine(char * str, struct oldrpmFileInfo * fi) {
    char ** fields;

    fields = splitString(str, strlen(str), ' ');

    fi->path = xstrdup(fields[0]);
    fi->state = RPMFILE_STATE_NORMAL;

    infoFromFields(fields + 1, fi);

    freeSplitString(fields);
}

void infoFromFields(char ** fields, struct oldrpmFileInfo * fi) {
    fi->size = strtol(fields[0], NULL, 10);
    fi->mtime = strtol(fields[1], NULL, 10);
    strcpy(fi->md5, fields[2]);
    fi->mode = strtol(fields[3], NULL, 8);
    fi->uid = strtol(fields[4], NULL, 10);
    fi->gid = strtol(fields[5], NULL, 10);
    fi->isconf = fields[6][0] != '0';
    fi->isdoc = fields[7][0] != '0';
    fi->rdev = strtol(fields[8], NULL, 16);
   
    if (S_ISLNK(fi->mode)) {
	fi->linkto = xstrdup(fields[9]);
    } else {
	fi->linkto = NULL;
    }
}

void oldrpmfileFree(struct oldrpmFileInfo * fi) {
    free(fi->path);
    if (fi->linkto) free(fi->linkto);
}

char * oldrpmfileToInfoStr(struct oldrpmFileInfo * fi) {
    char * buf;

    if (fi->linkto) 
	buf = xmalloc(strlen(fi->linkto) + 100);
    else
	buf = xmalloc(100);

    sprintf(buf, "%ld %ld %s %o %d %d %s %s %x ", (long)fi->size, (long)fi->mtime,
		fi->md5, fi->mode, (int)fi->uid, (int)fi->gid,
		fi->isconf ? "1" : "0", fi->isdoc ? "1" : "0",
		fi->rdev);
    
    if (fi->linkto) 
	strcat(buf, fi->linkto);
    else
	strcat(buf, "X");
 
    return buf;
}
