/** \ingroup rpmio
 * \file rpmio/rpmrpc.c
 */

#include "system.h"
#include <rpmio_internal.h>
#include <popt.h>
#include "ugid.h"
#include "debug.h"

/*@access FD_t@*/
/*@access urlinfo@*/

extern int _rpmio_debug;

/* =============================================================== */
static int ftpMkdir(const char * path, /*@unused@*/ mode_t mode) {
    int rc;
    if ((rc = ftpCmd("MKD", path, NULL)) != 0)
	return rc;
#if NOTYET
    {	char buf[20];
	sprintf(buf, " 0%o", mode);
	(void) ftpCmd("SITE CHMOD", path, buf);
    }
#endif
    return rc;
}

static int ftpChdir(const char * path) {
    return ftpCmd("CWD", path, NULL);
}

static int ftpRmdir(const char * path) {
    return ftpCmd("RMD", path, NULL);
}

static int ftpRename(const char * oldpath, const char * newpath) {
    int rc;
    if ((rc = ftpCmd("RNFR", oldpath, NULL)) != 0)
	return rc;
    return ftpCmd("RNTO", newpath, NULL);
}

static int ftpUnlink(const char * path) {
    return ftpCmd("DELE", path, NULL);
}

/* =============================================================== */
/* XXX rebuilddb.c: analogues to mkdir(2)/rmdir(2). */
int Mkdir (const char *path, mode_t mode) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpMkdir(path, mode);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return mkdir(path, mode);
}

int Chdir (const char *path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpChdir(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return chdir(path);
}

int Rmdir (const char *path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpRmdir(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return rmdir(path);
}

/* XXX rpmdb.c: analogue to rename(2). */

int Rename (const char *oldpath, const char * newpath) {
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    /* XXX lib/install.c used to rely on this behavior. */
    if (!strcmp(oldpath, newpath)) return 0;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_FTP:
if (_rpmio_debug)
fprintf(stderr, "*** rename old %*s new %*s\n", (int)(oe - oldpath), oldpath, (int)(ne - newpath), newpath);
	if (!(oldut == newut && oe && ne && (oe - oldpath) == (ne - newpath) &&
	    !xstrncasecmp(oldpath, newpath, (oe - oldpath))))
	    return -2;
	return ftpRename(oldpath, newpath);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return rename(oldpath, newpath);
}

int Link (const char *oldpath, const char * newpath) {
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
if (_rpmio_debug)
fprintf(stderr, "*** link old %*s new %*s\n", (int)(oe - oldpath), oldpath, (int)(ne - newpath), newpath);
	if (!(oldut == newut && oe && ne && (oe - oldpath) == (ne - newpath) &&
	    !xstrncasecmp(oldpath, newpath, (oe - oldpath))))
	    return -2;
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return link(oldpath, newpath);
}

/* XXX build/build.c: analogue to unlink(2). */

int Unlink(const char * path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpUnlink(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return unlink(path);
}

/* XXX swiped from mc-4.5.39-pre9 vfs/ftpfs.c */

#define	g_strdup	xstrdup
#define	g_free		free

/*
 * FIXME: this is broken. It depends on mc not crossing border on month!
 */
static int current_mday;
static int current_mon;
static int current_year;

/* Following stuff (parse_ls_lga) is used by ftpfs and extfs */
#define MAXCOLS		30

static char *columns [MAXCOLS];	/* Points to the string in column n */
static int   column_ptr [MAXCOLS]; /* Index from 0 to the starting positions of the columns */

static int
vfs_split_text (char *p)
{
    char *original = p;
    int  numcols;


    for (numcols = 0; *p && numcols < MAXCOLS; numcols++){
	while (*p == ' ' || *p == '\r' || *p == '\n'){
	    *p = 0;
	    p++;
	}
	columns [numcols] = p;
	column_ptr [numcols] = p - original;
	while (*p && *p != ' ' && *p != '\r' && *p != '\n')
	    p++;
    }
    return numcols;
}

static int
is_num (int idx)
{
    if (!columns [idx] || columns [idx][0] < '0' || columns [idx][0] > '9')
	return 0;
    return 1;
}

static int
is_dos_date(char *str)
{
    if (strlen(str) == 8 && str[2] == str[5] && strchr("\\-/", (int)str[2]) != NULL)
	return (1);

    return (0);
}

static int
is_week (char *str, struct tm *tim)
{
    static char *week = "SunMonTueWedThuFriSat";
    char *pos;

    if((pos=strstr(week, str)) != NULL){
        if(tim != NULL)
	    tim->tm_wday = (pos - week)/3;
	return (1);
    }
    return (0);    
}

static int
is_month (char *str, struct tm *tim)
{
    static char *month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char *pos;
    
    if((pos=strstr(month, str)) != NULL){
        if(tim != NULL)
	    tim->tm_mon = (pos - month)/3;
	return (1);
    }
    return (0);
}

static int
is_time (char *str, struct tm *tim)
{
    char *p, *p2;

    if ((p=strchr(str, ':')) && (p2=strrchr(str, ':'))) {
	if (p != p2) {
    	    if (sscanf (str, "%2d:%2d:%2d", &tim->tm_hour, &tim->tm_min, &tim->tm_sec) != 3)
		return (0);
	}
	else {
	    if (sscanf (str, "%2d:%2d", &tim->tm_hour, &tim->tm_min) != 2)
		return (0);
	}
    }
    else 
        return (0);
    
    return (1);
}

static int is_year(char *str, struct tm *tim)
{
    long year;

    if (strchr(str,':'))
        return (0);

    if (strlen(str)!=4)
        return (0);

    if (sscanf(str, "%ld", &year) != 1)
        return (0);

    if (year < 1900 || year > 3000)
        return (0);

    tim->tm_year = (int) (year - 1900);

    return (1);
}

/*
 * FIXME: this is broken. Consider following entry:
 * -rwx------   1 root     root            1 Aug 31 10:04 2904 1234
 * where "2904 1234" is filename. Well, this code decodes it as year :-(.
 */

static int
vfs_parse_filetype (char c)
{
    switch (c){
        case 'd': return S_IFDIR; 
        case 'b': return S_IFBLK;
        case 'c': return S_IFCHR;
        case 'l': return S_IFLNK;
        case 's':
#ifdef IS_IFSOCK /* And if not, we fall through to IFIFO, which is pretty close */
	          return S_IFSOCK;
#endif
        case 'p': return S_IFIFO;
        case 'm': case 'n':		/* Don't know what these are :-) */
        case '-': case '?': return S_IFREG;
        default: return -1;
    }
}

static int vfs_parse_filemode (char *p)
{	/* converts rw-rw-rw- into 0666 */
    int res = 0;
    switch (*(p++)){
	case 'r': res |= 0400; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0200; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0100; break;
	case 's': res |= 0100 | S_ISUID; break;
	case 'S': res |= S_ISUID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0040; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0020; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0010; break;
	case 's': res |= 0010 | S_ISGID; break;
        case 'l': /* Solaris produces these */
	case 'S': res |= S_ISGID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0004; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0002; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0001; break;
	case 't': res |= 0001 | S_ISVTX; break;
	case 'T': res |= S_ISVTX; break;
	case '-': break;
	default: return -1;
    }
    return res;
}

static int vfs_parse_filedate(int idx, time_t *t)
{	/* This thing parses from idx in columns[] array */

    char *p;
    struct tm tim;
    int d[3];
    int	got_year = 0;

    /* Let's setup default time values */
    tim.tm_year = current_year;
    tim.tm_mon  = current_mon;
    tim.tm_mday = current_mday;
    tim.tm_hour = 0;
    tim.tm_min  = 0;
    tim.tm_sec  = 0;
    tim.tm_isdst = -1; /* Let mktime() try to guess correct dst offset */
    
    p = columns [idx++];
    
    /* We eat weekday name in case of extfs */
    if(is_week(p, &tim))
	p = columns [idx++];

    /* Month name */
    if(is_month(p, &tim)){
	/* And we expect, it followed by day number */
	if (is_num (idx))
    	    tim.tm_mday = (int)atol (columns [idx++]);
	else
	    return 0; /* No day */

    } else {
        /* We usually expect:
           Mon DD hh:mm
           Mon DD  YYYY
	   But in case of extfs we allow these date formats:
           Mon DD YYYY hh:mm
	   Mon DD hh:mm YYYY
	   Wek Mon DD hh:mm:ss YYYY
           MM-DD-YY hh:mm
           where Mon is Jan-Dec, DD, MM, YY two digit day, month, year,
           YYYY four digit year, hh, mm, ss two digit hour, minute or second. */

	/* Here just this special case with MM-DD-YY */
        if (is_dos_date(p)){
            p[2] = p[5] = '-';
	    
	    if(sscanf(p, "%2d-%2d-%2d", &d[0], &d[1], &d[2]) == 3){
	    /*  We expect to get:
		1. MM-DD-YY
		2. DD-MM-YY
		3. YY-MM-DD
		4. YY-DD-MM  */
		
		/* Hmm... maybe, next time :)*/
		
		/* At last, MM-DD-YY */
		d[0]--; /* Months are zerobased */
		/* Y2K madness */
		if(d[2] < 70)
		    d[2] += 100;

        	tim.tm_mon  = d[0];
        	tim.tm_mday = d[1];
        	tim.tm_year = d[2];
		got_year = 1;
	    } else
		return 0; /* sscanf failed */
        } else
            return 0; /* unsupported format */
    }

    /* Here we expect to find time and/or year */
    
    if (is_num (idx)) {
        if(is_time(columns[idx], &tim) || (got_year = is_year(columns[idx], &tim))) {
	idx++;

	/* This is a special case for ctime() or Mon DD YYYY hh:mm */
	if(is_num (idx) && 
	    ((got_year = is_year(columns[idx], &tim)) || is_time(columns[idx], &tim)))
		idx++; /* time & year or reverse */
	} /* only time or date */
    }
    else 
        return 0; /* Nor time or date */

    /*
     * If the date is less than 6 months in the past, it is shown without year
     * other dates in the past or future are shown with year but without time
     * This does not check for years before 1900 ... I don't know, how
     * to represent them at all
     */
    if (!got_year &&
	current_mon < 6 && current_mon < tim.tm_mon && 
	tim.tm_mon - current_mon >= 6)

	tim.tm_year--;

    if ((*t = mktime(&tim)) < 0)
        *t = 0;
    return idx;
}

static int
vfs_parse_ls_lga (char *p, struct stat *st, char **filename, char **linkname)
{
    int idx, idx2, num_cols;
    int i;
    char *p_copy;
    
    if (strncmp (p, "total", 5) == 0)
        return 0;

    p_copy = g_strdup(p);
/* XXX FIXME: parse out inode number from "NLST -lai ." */
/* XXX FIXME: parse out sizein blocks from "NLST -lais ." */

    if ((i = vfs_parse_filetype(*(p++))) == -1)
        goto error;

    st->st_mode = i;
    if (*p == ' ')	/* Notwell 4 */
        p++;
    if (*p == '['){
	if (strlen (p) <= 8 || p [8] != ']')
	    goto error;
	/* Should parse here the Notwell permissions :) */
	if (S_ISDIR (st->st_mode))
	    st->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
	else
	    st->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
	p += 9;
    } else {
	if ((i = vfs_parse_filemode(p)) == -1)
	    goto error;
        st->st_mode |= i;
	p += 9;

        /* This is for an extra ACL attribute (HP-UX) */
        if (*p == '+')
            p++;
    }

    g_free(p_copy);
    p_copy = g_strdup(p);
    num_cols = vfs_split_text (p);

    st->st_nlink = atol (columns [0]);
    if (st->st_nlink < 0)
        goto error;

    if (!is_num (1))
#ifdef	HACK
	st->st_uid = finduid (columns [1]);
#else
	unameToUid (columns [1], &st->st_uid);
#endif
    else
        st->st_uid = (uid_t) atol (columns [1]);

    /* Mhm, the ls -lg did not produce a group field */
    for (idx = 3; idx <= 5; idx++) 
        if (is_month(columns [idx], NULL) || is_week(columns [idx], NULL) || is_dos_date(columns[idx]))
            break;

    if (idx == 6 || (idx == 5 && !S_ISCHR (st->st_mode) && !S_ISBLK (st->st_mode)))
	goto error;

    /* We don't have gid */	
    if (idx == 3 || (idx == 4 && (S_ISCHR(st->st_mode) || S_ISBLK (st->st_mode))))
        idx2 = 2;
    else { 
	/* We have gid field */
	if (is_num (2))
	    st->st_gid = (gid_t) atol (columns [2]);
	else
#ifdef	HACK
	    st->st_gid = findgid (columns [2]);
#else
	    gnameToGid (columns [1], &st->st_gid);
#endif
	idx2 = 3;
    }

    /* This is device */
    if (S_ISCHR (st->st_mode) || S_ISBLK (st->st_mode)){
	int maj, min;
	
	if (!is_num (idx2) || sscanf(columns [idx2], " %d,", &maj) != 1)
	    goto error;
	
	if (!is_num (++idx2) || sscanf(columns [idx2], " %d", &min) != 1)
	    goto error;
	
#ifdef HAVE_ST_RDEV
	st->st_rdev = ((maj & 0xff) << 8) | (min & 0xffff00ff);
#endif
	st->st_size = 0;
	
    } else {
	/* Common file size */
	if (!is_num (idx2))
	    goto error;
	
	st->st_size = (size_t) atol (columns [idx2]);
#ifdef HAVE_ST_RDEV
	st->st_rdev = 0;
#endif
    }

    idx = vfs_parse_filedate(idx, &st->st_mtime);
    if (!idx)
        goto error;
    /* Use resulting time value */
    st->st_atime = st->st_ctime = st->st_mtime;
    st->st_dev = 0;
    st->st_ino = 0;
#ifdef HAVE_ST_BLKSIZE
    st->st_blksize = 512;
#endif
#ifdef HAVE_ST_BLOCKS
    st->st_blocks = (st->st_size + 511) / 512;
#endif

    for (i = idx + 1, idx2 = 0; i < num_cols; i++ ) 
	if (strcmp (columns [i], "->") == 0){
	    idx2 = i;
	    break;
	}
    
    if (((S_ISLNK (st->st_mode) || 
        (num_cols == idx + 3 && st->st_nlink > 1))) /* Maybe a hardlink? (in extfs) */
        && idx2){
	int tlen;
	char *t;
	    
	if (filename){
#ifdef HACK
	    t = g_strndup (p_copy + column_ptr [idx], column_ptr [idx2] - column_ptr [idx] - 1);
#else
	    int nb = column_ptr [idx2] - column_ptr [idx] - 1;
	    t = xmalloc(nb+1);
	    strncpy(t, p_copy + column_ptr [idx], nb);
#endif
	    *filename = t;
	}
	if (linkname){
	    t = g_strdup (p_copy + column_ptr [idx2+1]);
	    tlen = strlen (t);
	    if (t [tlen-1] == '\r' || t [tlen-1] == '\n')
		t [tlen-1] = 0;
	    if (t [tlen-2] == '\r' || t [tlen-2] == '\n')
		t [tlen-2] = 0;
		
	    *linkname = t;
	}
    } else {
	/* Extract the filename from the string copy, not from the columns
	 * this way we have a chance of entering hidden directories like ". ."
	 */
	if (filename){
	    /* 
	    *filename = g_strdup (columns [idx++]);
	    */
	    int tlen;
	    char *t;
	    
	    t = g_strdup (p_copy + column_ptr [idx++]);
	    tlen = strlen (t);
	    /* g_strchomp(); */
	    if (t [tlen-1] == '\r' || t [tlen-1] == '\n')
	        t [tlen-1] = 0;
	    if (t [tlen-2] == '\r' || t [tlen-2] == '\n')
		t [tlen-2] = 0;
	    
	    *filename = t;
	}
	if (linkname)
	    *linkname = NULL;
    }
    g_free (p_copy);
    return 1;

error:
#ifdef	HACK
    {
      static int errorcount = 0;

      if (++errorcount < 5) {
	message_1s (1, "Could not parse:", p_copy);
      } else if (errorcount == 5)
	message_1s (1, "More parsing errors will be ignored.", "(sorry)" );
    }
#endif

    if (p_copy != p)		/* Carefull! */
	g_free (p_copy);
    return 0;
}

typedef enum {
	DO_FTP_STAT	= 1,
	DO_FTP_LSTAT	= 2,
	DO_FTP_READLINK	= 3,
	DO_FTP_ACCESS	= 4,
	DO_FTP_GLOB	= 5
} ftpSysCall_t;
static size_t ftpBufAlloced = 0;
static /*@only@*/ char * ftpBuf = NULL;
	
#define alloca_strdup(_s)       strcpy(alloca(strlen(_s)+1), (_s))

static int ftpNLST(const char * url, ftpSysCall_t ftpSysCall,
	struct stat * st, char * rlbuf, size_t rlbufsiz)
{
    FD_t fd;
    const char * path;
    int bufLength, moretodo;
    const char *n, *ne, *o, *oe;
    char * s;
    char * se;
    const char * urldn;
    char * bn = NULL;
    int nbn = 0;
    urlinfo u;
    int rc;

    n = ne = o = oe = NULL;
    (void) urlPath(url, &path);
    if (*path == '\0')
	return -2;

    switch (ftpSysCall) {
    case DO_FTP_GLOB:
	fd = ftpOpen(url, 0, 0, &u);
	if (fd == NULL || u == NULL)
	    return -1;

	u->openError = ftpReq(fd, "NLST", path);
	break;
    default:
	urldn = alloca_strdup(url);
	if ((bn = strrchr(urldn, '/')) == NULL)
	    return -2;
	else if (bn == path)
	    bn = ".";
	else
	    *bn++ = '\0';
	nbn = strlen(bn);

	rc = ftpChdir(urldn);		/* XXX don't care about CWD */
	if (rc < 0)
	    return rc;

	fd = ftpOpen(url, 0, 0, &u);
	if (fd == NULL || u == NULL)
	    return -1;

	/* XXX possibly should do "NLST -lais" to get st_ino/st_blocks also */
	u->openError = ftpReq(fd, "NLST", "-la");
	break;
    }

    if (u->openError < 0) {
	fd = fdLink(fd, "error data (ftpStat)");
	rc = -2;
	goto exit;
    }

    if (ftpBufAlloced == 0 || ftpBuf == NULL) {
        ftpBufAlloced = url_iobuf_size;
        ftpBuf = xcalloc(ftpBufAlloced, sizeof(ftpBuf[0]));
    }
    *ftpBuf = '\0';

    bufLength = 0;
    moretodo = 1;

    do {

	/* XXX FIXME: realloc ftpBuf is < ~128 chars remain */
	if ((ftpBufAlloced - bufLength) < (1024+80)) {
	    ftpBufAlloced <<= 2;
	    ftpBuf = xrealloc(ftpBuf, ftpBufAlloced);
	}
	s = se = ftpBuf + bufLength;
	*se = '\0';

	rc = fdFgets(fd, se, (ftpBufAlloced - bufLength));
	if (rc <= 0) {
	    moretodo = 0;
	    break;
	}
	if (ftpSysCall == DO_FTP_GLOB) {	/* XXX HACK */
	    bufLength += strlen(se);
	    continue;
	}

	for (s = se; *s != '\0'; s = se) {
	    int bingo;

	    while (*se && *se != '\n') se++;
	    if (se > s && se[-1] == '\r') se[-1] = '\0';
	    if (*se == '\0') break;
	    *se++ = '\0';

	    if (!strncmp(s, "total ", sizeof("total ")-1)) continue;

	    o = NULL;
	    for (bingo = 0, n = se; n >= s; n--) {
		switch (*n) {
		case '\0':
		    oe = ne = n;
		    break;
		case ' ':
		    if (o || !(n[-3] == ' ' && n[-2] == '-' && n[-1] == '>')) {
			while (*(++n) == ' ');
			bingo++;
			break;
		    }
		    for (o = n + 1; *o == ' '; o++);
		    n -= 3;
		    ne = n;
		    break;
		default:
		    break;
		}
		if (bingo) break;
	    }

	    if (nbn != (ne - n))	continue;	/* Same name length? */
	    if (strncmp(n, bn, nbn))	continue;	/* Same name? */

	    moretodo = 0;
	    break;
	}

        if (moretodo && se > s) {
            bufLength = se - s - 1;
            if (s != ftpBuf)
                memmove(ftpBuf, s, bufLength);
        } else {
	    bufLength = 0;
	}
    } while (moretodo);

    switch (ftpSysCall) {
    case DO_FTP_STAT:
	if (o && oe) {
	    /* XXX FIXME: symlink, replace urldn/bn from [o,oe) and restart */
	}
	/*@fallthrough@*/
    case DO_FTP_LSTAT:
	if (st == NULL || !(n && ne)) {
	    rc = -1;
	} else {
	    rc = ((vfs_parse_ls_lga(s, st, NULL, NULL) > 0) ? 0 : -1);
	}
	break;
    case DO_FTP_READLINK:
	if (rlbuf == NULL || !(o && oe)) {
	    rc = -1;
	} else {
	    rc = oe - o;
	    if (rc > rlbufsiz)
		rc = rlbufsiz;
	    memcpy(rlbuf, o, rc);
	    if (rc < rlbufsiz)
		rlbuf[rc] = '\0';
	}
	break;
    case DO_FTP_ACCESS:
	rc = 0;		/* XXX WRONG WRONG WRONG */
	break;
    case DO_FTP_GLOB:
	rc = 0;		/* XXX WRONG WRONG WRONG */
	break;
    }

exit:
    ufdClose(fd);
    return rc;
}

static int ftpStat(const char * path, struct stat *st)
{
    return ftpNLST(path, DO_FTP_STAT, st, NULL, 0);
}

static int ftpLstat(const char * path, struct stat *st) {
    int rc;
    rc = ftpNLST(path, DO_FTP_LSTAT, st, NULL, 0);
if (_rpmio_debug)
fprintf(stderr, "*** ftpLstat(%s) rc %d\n", path, rc);
    return rc;
}

static int ftpReadlink(const char * path, char * buf, size_t bufsiz) {
    return ftpNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
}

static int ftpGlob(const char * path, int flags,
		int errfunc(const char * epath, int eerno), glob_t * pglob)
{
    int rc;

    if (pglob == NULL)
	return -2;
    rc = ftpNLST(path, DO_FTP_GLOB, NULL, NULL, 0);
if (_rpmio_debug)
fprintf(stderr, "*** ftpGlob(%s,0x%x,%p,%p) ftpNLST rc %d\n", path, (unsigned)flags, errfunc, pglob, rc);
    if (rc)
	return rc;
    rc = poptParseArgvString(ftpBuf, &pglob->gl_pathc, (const char ***)&pglob->gl_pathv);
    pglob->gl_offs = -1;	/* XXX HACK HACK HACK */
    return rc;
}

static void ftpGlobfree(glob_t * pglob) {
if (_rpmio_debug)
fprintf(stderr, "*** ftpGlobfree(%p)\n", pglob);
    if (pglob->gl_offs == -1)	/* XXX HACK HACK HACK */
	free((void *)pglob->gl_pathv);
}

int Stat(const char * path, struct stat * st) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Stat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_FTP:
	return ftpStat(path, st);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return stat(path, st);
}

int Lstat(const char * path, struct stat * st) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Lstat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_FTP:
	return ftpLstat(path, st);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return lstat(path, st);
}

int Readlink(const char * path, char * buf, size_t bufsiz) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpReadlink(path, buf, bufsiz);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return readlink(path, buf, bufsiz);
}

int Access(const char * path, int amode) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Access(%s,%d)\n", path, amode);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return access(path, amode);
}

int Glob(const char *path, int flags,
	int errfunc(const char * epath, int eerrno), glob_t *pglob)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Glob(%s,0x%x,%p,%p)\n", path, (unsigned)flags, errfunc, pglob);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
	return ftpGlob(path, flags, errfunc, pglob);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return glob(path, flags, errfunc, pglob);
}

void Globfree(glob_t *pglob)
{
if (_rpmio_debug)
fprintf(stderr, "*** Globfree(%p)\n", pglob);
    if (pglob->gl_offs == -1) /* XXX HACK HACK HACK */
	ftpGlobfree(pglob);
    else
	globfree(pglob);
}

DIR * Opendir(const char * path)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Opendir(%s)\n", path);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return NULL;
	/*@notreached@*/ break;
    }
    return opendir(path);
}

struct dirent * Readdir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Readdir(%p)\n", dir);
    return readdir(dir);
}

int Closedir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Closedir(%p)\n", dir);
    return closedir(dir);
}
