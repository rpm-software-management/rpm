/*
 * apprentice - make one pass through /etc/magic, learning its secrets.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include "system.h"
#include "file.h"
#include "debug.h"

FILE_RCSID("@(#)Id: apprentice.c,v 1.49 2002/07/03 19:00:41 christos Exp ")

/*@access fmagic @*/

#define	EATAB {while (isascii((unsigned char) *l) && \
		      isspace((unsigned char) *l))  ++l;}
#define LOWCASE(l) (isupper((unsigned char) (l)) ? \
			tolower((unsigned char) (l)) : (l))
/*
 * Work around a bug in headers on Digital Unix.
 * At least confirmed for: OSF1 V4.0 878
 */
#if defined(__osf__) && defined(__DECC)
#ifdef MAP_FAILED
#undef MAP_FAILED
#endif
#endif

#ifndef MAP_FAILED
#define MAP_FAILED (void *) -1
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/*@unchecked@*/
#ifdef __EMX__
  static char PATHSEP=';';
#else
  static char PATHSEP=':';
#endif

/*@unchecked@*/
static int maxmagic = 0;

#ifndef MAGIC
# define MAGIC "/etc/magic"
#endif

/*@unchecked@*/ /*@observer@*/
const char *default_magicfile = MAGIC;

/*
 * extend the sign bit if the comparison is to be signed
 */
uint32_t
file_signextend(struct magic *m, uint32_t v)
{
	if (!(m->flag & UNSIGNED))
		switch(m->type) {
		/*
		 * Do not remove the casts below.  They are
		 * vital.  When later compared with the data,
		 * the sign extension must have happened.
		 */
		case FILE_BYTE:
			v = (char) v;
			break;
		case FILE_SHORT:
		case FILE_BESHORT:
		case FILE_LESHORT:
			v = (short) v;
			break;
		case FILE_DATE:
		case FILE_BEDATE:
		case FILE_LEDATE:
		case FILE_LDATE:
		case FILE_BELDATE:
		case FILE_LELDATE:
		case FILE_LONG:
		case FILE_BELONG:
		case FILE_LELONG:
			v = (int32_t) v;
			break;
		case FILE_STRING:
		case FILE_PSTRING:
			break;
		case FILE_REGEX:
			break;
		default:
			file_magwarn("can't happen: m->type=%d\n", m->type);
			return -1;
		}
	return v;
}

/*
 * eatsize(): Eat the size spec from a number [eg. 10UL]
 */
/*@-bounds@*/
static void
eatsize(/*@out@*/ char **p)
	/*@modifies *p @*/
{
	char *l = *p;

	if (LOWCASE(*l) == 'u') 
		l++;

	switch (LOWCASE(*l)) {
	case 'l':    /* long */
	case 's':    /* short */
	case 'h':    /* short */
	case 'b':    /* char/byte */
	case 'c':    /* char/byte */
		l++;
		/*@fallthrough@*/
	default:
		break;
	}

	*p = l;
}
/*@=bounds@*/

/* Single hex char to int; -1 if not a hex char. */
static int
hextoint(int c)
	/*@*/
{
	if (!isascii((unsigned char) c))
		return -1;
	if (isdigit((unsigned char) c))
		return c - '0';
	if ((c >= 'a')&&(c <= 'f'))
		return c + 10 - 'a';
	if (( c>= 'A')&&(c <= 'F'))
		return c + 10 - 'A';
	return -1;
}

/*
 * Convert a string containing C character escapes.  Stop at an unescaped
 * space or tab.
 * Copy the converted version to "p", returning its length in *slen.
 * Return updated scan pointer as function result.
 */
/*@-shiftimplementation@*/
/*@-bounds@*/
static char *
getstr(/*@returned@*/ char *s, char *p, int plen, /*@out@*/ int *slen)
	/*@globals fileSystem @*/
	/*@modifies *p, *slen, fileSystem @*/
{
	char	*origs = s, *origp = p;
	char	*pmax = p + plen - 1;
	int	c;
	int	val;

	while ((c = *s++) != '\0') {
		if (isspace((unsigned char) c))
			break;
		if (p >= pmax) {
			fprintf(stderr, "String too long: %s\n", origs);
			break;
		}
		if(c == '\\') {
			switch(c = *s++) {

			case '\0':
				goto out;

			default:
				*p++ = (char) c;
				/*@switchbreak@*/ break;

			case 'n':
				*p++ = '\n';
				/*@switchbreak@*/ break;

			case 'r':
				*p++ = '\r';
				/*@switchbreak@*/ break;

			case 'b':
				*p++ = '\b';
				/*@switchbreak@*/ break;

			case 't':
				*p++ = '\t';
				/*@switchbreak@*/ break;

			case 'f':
				*p++ = '\f';
				/*@switchbreak@*/ break;

			case 'v':
				*p++ = '\v';
				/*@switchbreak@*/ break;

			/* \ and up to 3 octal digits */
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				val = c - '0';
				c = *s++;  /* try for 2 */
				if(c >= '0' && c <= '7') {
					val = (val<<3) | (c - '0');
					c = *s++;  /* try for 3 */
					if(c >= '0' && c <= '7')
						val = (val<<3) | (c-'0');
					else
						--s;
				}
				else
					--s;
				*p++ = (char)val;
				/*@switchbreak@*/ break;

			/* \x and up to 2 hex digits */
			case 'x':
				val = 'x';	/* Default if no digits */
				c = hextoint(*s++);	/* Get next char */
				if (c >= 0) {
					val = c;
					c = hextoint(*s++);
					if (c >= 0)
						val = (val << 4) + c;
					else
						--s;
				} else
					--s;
				*p++ = (char)val;
				/*@switchbreak@*/ break;
			}
		} else
			*p++ = (char)c;
	}
out:
	*p = '\0';
	*slen = p - origp;
	return s;
}
/*@=bounds@*/
/*@=shiftimplementation@*/

/* 
 * Read a numeric value from a pointer, into the value union of a magic 
 * pointer, according to the magic type.  Update the string pointer to point 
 * just after the number read.  Return 0 for success, non-zero for failure.
 */
/*@-bounds@*/
static int
getvalue(struct magic *m, /*@out@*/ char **p)
	/*@globals fileSystem @*/
	/*@modifies m, *p, fileSystem @*/
{
	int slen;

	if (m->type == FILE_STRING || m->type == FILE_PSTRING || m->type == FILE_REGEX) {
		*p = getstr(*p, m->value.s, sizeof(m->value.s), &slen);
		m->vallen = slen;
	} else
		if (m->reln != 'x') {
			m->value.l = file_signextend(m, strtoul(*p, p, 0));
			eatsize(p);
		}
	return 0;
}
/*@=bounds@*/

/*
 * parse one line from magic file, put into magic[index++] if valid
 */
/*@-bounds@*/
static int
parse(/*@out@*/ struct magic **magicp, /*@out@*/ uint32_t *nmagicp,
		char *l, int action)
	/*@globals maxmagic, fileSystem @*/
	/*@modifies *magicp, *nmagicp, maxmagic, fileSystem @*/
{
	int i = 0;
	struct magic *m;
	char *t;

#define ALLOC_INCR	200
	if (*nmagicp + 1 >= maxmagic){
		maxmagic += ALLOC_INCR;
/*@-unqualifiedtrans @*/
		*magicp = xrealloc(*magicp, sizeof(**magicp) * maxmagic);
/*@=unqualifiedtrans @*/
		m = &(*magicp)[*nmagicp];
		memset(m, 0, sizeof(**magicp) * ALLOC_INCR);
	} else
		m = &(*magicp)[*nmagicp];
	m->flag = 0;
	m->cont_level = 0;

	while (*l == '>') {
		++l;		/* step over */
		m->cont_level++; 
	}

	if (m->cont_level != 0 && *l == '(') {
		++l;		/* step over */
		m->flag |= INDIR;
	}
	if (m->cont_level != 0 && *l == '&') {
                ++l;            /* step over */
                m->flag |= OFFADD;
        }

	/* get offset, then skip over it */
	m->offset = (int) strtoul(l,&t,0);
        if (l == t)
		file_magwarn("offset %s invalid", l);
        l = t;

	if (m->flag & INDIR) {
		m->in_type = FILE_LONG;
		m->in_offset = 0;
		/*
		 * read [.lbs][+-]nnnnn)
		 */
		if (*l == '.') {
			l++;
			switch (*l) {
			case 'l':
				m->in_type = FILE_LELONG;
				break;
			case 'L':
				m->in_type = FILE_BELONG;
				break;
			case 'h':
			case 's':
				m->in_type = FILE_LESHORT;
				break;
			case 'H':
			case 'S':
				m->in_type = FILE_BESHORT;
				break;
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				m->in_type = FILE_BYTE;
				break;
			default:
				file_magwarn("indirect offset type %c invalid", *l);
				break;
			}
			l++;
		}
		if (*l == '~') {
			m->in_op = FILE_OPINVERSE;
			l++;
		}
		switch (*l) {
		case '&':
			m->in_op |= FILE_OPAND;
			l++;
			break;
		case '|':
			m->in_op |= FILE_OPOR;
			l++;
			break;
		case '^':
			m->in_op |= FILE_OPXOR;
			l++;
			break;
		case '+':
			m->in_op |= FILE_OPADD;
			l++;
			break;
		case '-':
			m->in_op |= FILE_OPMINUS;
			l++;
			break;
		case '*':
			m->in_op |= FILE_OPMULTIPLY;
			l++;
			break;
		case '/':
			m->in_op |= FILE_OPDIVIDE;
			l++;
			break;
		case '%':
			m->in_op |= FILE_OPMODULO;
			l++;
			break;
		}
		if (isdigit((unsigned char)*l)) 
			m->in_offset = strtoul(l, &t, 0);
		else
			t = l;
		if (*t++ != ')') 
			file_magwarn("missing ')' in indirect offset");
		l = t;
	}


	while (isascii((unsigned char)*l) && isdigit((unsigned char)*l))
		++l;
	EATAB;

#define NBYTE		4
#define NSHORT		5
#define NLONG		4
#define NSTRING 	6
#define NDATE		4
#define NBESHORT	7
#define NBELONG		6
#define NBEDATE		6
#define NLESHORT	7
#define NLELONG		6
#define NLEDATE		6
#define NPSTRING	7
#define NLDATE		5
#define NBELDATE	7
#define NLELDATE	7
#define NREGEX		5

	if (*l == 'u') {
		++l;
		m->flag |= UNSIGNED;
	}

	/* get type, skip it */
	if (strncmp(l, "char", NBYTE)==0) {	/* HP/UX compat */
		m->type = FILE_BYTE;
		l += NBYTE;
	} else if (strncmp(l, "byte", NBYTE)==0) {
		m->type = FILE_BYTE;
		l += NBYTE;
	} else if (strncmp(l, "short", NSHORT)==0) {
		m->type = FILE_SHORT;
		l += NSHORT;
	} else if (strncmp(l, "long", NLONG)==0) {
		m->type = FILE_LONG;
		l += NLONG;
	} else if (strncmp(l, "string", NSTRING)==0) {
		m->type = FILE_STRING;
		l += NSTRING;
	} else if (strncmp(l, "date", NDATE)==0) {
		m->type = FILE_DATE;
		l += NDATE;
	} else if (strncmp(l, "beshort", NBESHORT)==0) {
		m->type = FILE_BESHORT;
		l += NBESHORT;
	} else if (strncmp(l, "belong", NBELONG)==0) {
		m->type = FILE_BELONG;
		l += NBELONG;
	} else if (strncmp(l, "bedate", NBEDATE)==0) {
		m->type = FILE_BEDATE;
		l += NBEDATE;
	} else if (strncmp(l, "leshort", NLESHORT)==0) {
		m->type = FILE_LESHORT;
		l += NLESHORT;
	} else if (strncmp(l, "lelong", NLELONG)==0) {
		m->type = FILE_LELONG;
		l += NLELONG;
	} else if (strncmp(l, "ledate", NLEDATE)==0) {
		m->type = FILE_LEDATE;
		l += NLEDATE;
	} else if (strncmp(l, "pstring", NPSTRING)==0) {
		m->type = FILE_PSTRING;
		l += NPSTRING;
	} else if (strncmp(l, "ldate", NLDATE)==0) {
		m->type = FILE_LDATE;
		l += NLDATE;
	} else if (strncmp(l, "beldate", NBELDATE)==0) {
		m->type = FILE_BELDATE;
		l += NBELDATE;
	} else if (strncmp(l, "leldate", NLELDATE)==0) {
		m->type = FILE_LELDATE;
		l += NLELDATE;
	} else if (strncmp(l, "regex", NREGEX)==0) {
		m->type = FILE_REGEX;
		l += sizeof("regex");
	} else {
		file_magwarn("type %s invalid", l);
		return -1;
	}
	/* New-style anding: "0 byte&0x80 =0x80 dynamically linked" */
	/* New and improved: ~ & | ^ + - * / % -- exciting, isn't it? */
	if (*l == '~') {
		if (m->type != FILE_STRING && m->type != FILE_PSTRING)
			m->mask_op = FILE_OPINVERSE;
		++l;
	}
	switch (*l) {
	case '&':
		m->mask_op |= FILE_OPAND;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '|':
		m->mask_op |= FILE_OPOR;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '^':
		m->mask_op |= FILE_OPXOR;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '+':
		m->mask_op |= FILE_OPADD;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '-':
		m->mask_op |= FILE_OPMINUS;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '*':
		m->mask_op |= FILE_OPMULTIPLY;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '%':
		m->mask_op |= FILE_OPMODULO;
		++l;
		m->mask = file_signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
		break;
	case '/':
		if (m->type != FILE_STRING && m->type != FILE_PSTRING) {
			m->mask_op |= FILE_OPDIVIDE;
			++l;
			m->mask = file_signextend(m, strtoul(l, &l, 0));
			eatsize(&l);
		} else {
			m->mask = 0L;
			while (!isspace(*++l)) {
				switch (*l) {
				case CHAR_IGNORE_LOWERCASE:
					m->mask |= STRING_IGNORE_LOWERCASE;
					/*@switchbreak@*/ break;
				case CHAR_COMPACT_BLANK:
					m->mask |= STRING_COMPACT_BLANK;
					/*@switchbreak@*/ break;
				case CHAR_COMPACT_OPTIONAL_BLANK:
					m->mask |=
					    STRING_COMPACT_OPTIONAL_BLANK;
					/*@switchbreak@*/ break;
				default:
					file_magwarn("string extension %c invalid",
					    *l);
					return -1;
				}
			}
		}
		break;
	}
	/* We used to set mask to all 1's here, instead let's just not do anything 
	   if mask = 0 (unless you have a better idea) */
	EATAB;
  
	switch (*l) {
	case '>':
	case '<':
	/* Old-style anding: "0 byte &0x80 dynamically linked" */
	case '&':
	case '^':
	case '=':
  		m->reln = *l;
  		++l;
		if (*l == '=') {
		   /* HP compat: ignore &= etc. */
		   ++l;
		}
		break;
	case '!':
		if (m->type != FILE_STRING && m->type != FILE_PSTRING) {
			m->reln = *l;
			++l;
			break;
		}
		/*@fallthrough@*/
	default:
		if (*l == 'x' && isascii((unsigned char)l[1]) && 
		    isspace((unsigned char)l[1])) {
			m->reln = *l;
			++l;
			goto GetDesc;	/* Bill The Cat */
		}
  		m->reln = '=';
		break;
	}
  	EATAB;
  
	if (getvalue(m, &l))
		return -1;
	/*
	 * TODO finish this macro and start using it!
	 * #define offsetcheck {if (offset > HOWMANY-1) 
	 *	file_magwarn("offset too big"); }
	 */

	/*
	 * now get last part - the description
	 */
GetDesc:
	EATAB;
	if (l[0] == '\b') {
		++l;
		m->nospflag = 1;
	} else if ((l[0] == '\\') && (l[1] == 'b')) {
		++l;
		++l;
		m->nospflag = 1;
	} else
		m->nospflag = 0;
	while ((m->desc[i++] = *l++) != '\0' && i<MAXDESC)
		{};

#ifndef COMPILE_ONLY
	if (action == FILE_CHECK) {
		file_mdump(m);
	}
#endif
	++(*nmagicp);		/* make room for next */
	return 0;
}
/*@=bounds@*/

/*
 * Print a string containing C character escapes.
 */
void
file_showstr(FILE *fp, const char *s, size_t len)
{
	char	c;

	for (;;) {
		c = *s++;
		if (len == -1) {
			if (c == '\0')
				break;
		}
		else  {
			if (len-- == 0)
				break;
		}
		if(c >= 040 && c <= 0176)	/* TODO isprint && !iscntrl */
			(void) fputc(c, fp);
		else {
			(void) fputc('\\', fp);
			switch (c) {
			
			case '\n':
				(void) fputc('n', fp);
				/*@switchbreak@*/ break;

			case '\r':
				(void) fputc('r', fp);
				/*@switchbreak@*/ break;

			case '\b':
				(void) fputc('b', fp);
				/*@switchbreak@*/ break;

			case '\t':
				(void) fputc('t', fp);
				/*@switchbreak@*/ break;

			case '\f':
				(void) fputc('f', fp);
				/*@switchbreak@*/ break;

			case '\v':
				(void) fputc('v', fp);
				/*@switchbreak@*/ break;

			default:
				(void) fprintf(fp, "%.3o", c & 0377);
				/*@switchbreak@*/ break;
			}
		}
	}
}

/*
 * swap a short
 */
/*@-bounds@*/
static uint16_t
swap2(uint16_t sv)
	/*@*/
{
	uint16_t rv;
	uint8_t *s = (uint8_t *) &sv; 
	uint8_t *d = (uint8_t *) &rv; 
	d[0] = s[1];
	d[1] = s[0];
	return rv;
}
/*@=bounds@*/

/*
 * swap an int
 */
/*@-bounds@*/
static uint32_t
swap4(uint32_t sv)
	/*@*/
{
	uint32_t rv;
	uint8_t *s = (uint8_t *) &sv; 
	uint8_t *d = (uint8_t *) &rv; 
	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];
	return rv;
}
/*@=bounds@*/

/*
 * byteswap a single magic entry
 */
static
void bs1(struct magic *m)
	/*@modifies m @*/
{
	m->cont_level = swap2(m->cont_level);
	m->offset = swap4(m->offset);
	m->in_offset = swap4(m->in_offset);
	if (m->type != FILE_STRING)
		m->value.l = swap4(m->value.l);
	m->mask = swap4(m->mask);
}

/*
 * Byteswap an mmap'ed file if needed
 */
/*@-bounds@*/
static void
byteswap(/*@null@*/ struct magic *m, uint32_t nmagic)
	/*@modifies m @*/
{
	uint32_t i;
	if (m != NULL)
	for (i = 0; i < nmagic; i++)
		bs1(&m[i]);
}
/*@=bounds@*/

/*
 * make a dbname
 */
static char *
mkdbname(const char *fn)
	/*@*/
{
	static const char ext[] = ".mgc";
	/*@only@*/
	static char *buf = NULL;

	buf = xrealloc(buf, strlen(fn) + sizeof(ext) + 1);
	(void)strcpy(buf, fn);
	(void)strcat(buf, ext);
	return buf;
}

/*
 * parse from a file
 * const char *fn: name of magic file
 */
/*@-bounds@*/
static int
apprentice_file(fmagic fm, /*@out@*/ struct magic **magicp,
		/*@out@*/ uint32_t *nmagicp, const char *fn, int action)
	/*@globals maxmagic, fileSystem @*/
	/*@modifies fm, *magicp, *nmagicp, maxmagic, fileSystem @*/
{
	/*@observer@*/
	static const char hdr[] =
		"cont\toffset\ttype\topcode\tmask\tvalue\tdesc";
	FILE *f;
	char line[BUFSIZ+1];
	int errs = 0;

	f = fopen(fn, "r");
	if (f == NULL) {
		if (errno != ENOENT)
			(void) fprintf(stderr,
			    "%s: can't read magic file %s (%s)\n", 
			    __progname, fn, strerror(errno));
		return -1;
	}

        maxmagic = MAXMAGIS;
	*magicp = (struct magic *) xcalloc(sizeof(**magicp), maxmagic);

	/* parse it */
	if (action == FILE_CHECK)	/* print silly verbose header for USG compat. */
		(void) printf("%s\n", hdr);

	for (fm->lineno = 1; fgets(line, BUFSIZ, f) != NULL; fm->lineno++) {
		if (line[0]=='#')	/* comment, do not parse */
			continue;
		if (strlen(line) <= (unsigned)1) /* null line, garbage, etc */
			continue;
		line[strlen(line)-1] = '\0'; /* delete newline */
		if (parse(magicp, nmagicp, line, action) != 0)
			errs = 1;
	}

	(void) fclose(f);
	if (errs) {
		free(*magicp);
		*magicp = NULL;
		*nmagicp = 0;
	}
	return errs;
}
/*@=bounds@*/

/*
 * handle an mmaped file.
 */
/*@-bounds@*/
static int
apprentice_compile(/*@unused@*/ const fmagic fm,
		/*@out@*/ struct magic **magicp, /*@out@*/ uint32_t *nmagicp,
		const char *fn, /*@unused@*/ int action)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
	int fd;
	char *dbname = mkdbname(fn);
	/*@observer@*/
	static const uint32_t ar[] = {
	    MAGICNO, VERSIONNO
	};

	if (dbname == NULL) 
		return -1;

	if ((fd = open(dbname, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		(void)fprintf(stderr, "%s: Cannot open `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		return -1;
	}

	if (write(fd, ar, sizeof(ar)) != sizeof(ar)) {
		(void)fprintf(stderr, "%s: error writing `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		return -1;
	}

	if (lseek(fd, sizeof(**magicp), SEEK_SET) != sizeof(**magicp)) {
		(void)fprintf(stderr, "%s: error seeking `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		return -1;
	}

	if (write(fd, *magicp,  sizeof(**magicp) * *nmagicp) 
	    != sizeof(**magicp) * *nmagicp) {
		(void)fprintf(stderr, "%s: error writing `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		return -1;
	}

	(void)close(fd);
	return 0;
}
/*@=bounds@*/

/*
 * handle a compiled file.
 */
/*@-bounds@*/
static int
apprentice_map(/*@unused@*/ const fmagic fm,
		/*@out@*/ struct magic **magicp, /*@out@*/ uint32_t *nmagicp,
		const char *fn, /*@unused@*/ int action)
	/*@globals fileSystem, internalState @*/
	/*@modifies *magicp, *nmagicp, fileSystem, internalState @*/
{
	int fd;
	struct stat st;
	uint32_t *ptr;
	uint32_t version;
	int needsbyteswap;
	char *dbname = mkdbname(fn);
	void *mm = NULL;

	if (dbname == NULL)
		return -1;

	if ((fd = open(dbname, O_RDONLY)) == -1)
		return -1;

	if (fstat(fd, &st) == -1) {
		(void)fprintf(stderr, "%s: Cannot stat `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		goto errxit;
	}

#ifdef HAVE_MMAP
	if ((mm = mmap(0, (size_t)st.st_size, PROT_READ|PROT_WRITE,
	    MAP_PRIVATE|MAP_FILE, fd, (off_t)0)) == MAP_FAILED) {
		(void)fprintf(stderr, "%s: Cannot map `%s' (%s)\n",
		    __progname, dbname, strerror(errno));
		goto errxit;
	}
#else
	mm = xmalloc((size_t)st.st_size);
	if (read(fd, mm, (size_t)st.st_size) != (size_t)st.st_size) {
		(void) fprintf(stderr, "%s: Read failed (%s).\n", __progname,
		    strerror(errno));
		goto errxit;
	}
#endif
	*magicp = mm;
	(void)close(fd);
	fd = -1;
	ptr = (uint32_t *) *magicp;
	if (ptr == NULL)	/* XXX can't happen */
		goto errxit;
	if (*ptr != MAGICNO) {
		if (swap4(*ptr) != MAGICNO) {
			(void)fprintf(stderr, "%s: Bad magic in `%s'\n",
			    __progname, dbname);
			goto errxit;
		}
		needsbyteswap = 1;
	} else
		needsbyteswap = 0;
	if (needsbyteswap)
		version = swap4(ptr[1]);
	else
		version = ptr[1];
	if (version != VERSIONNO) {
		(void)fprintf(stderr, 
		    "%s: version mismatch (%d != %d) in `%s'\n",
		    __progname, version, VERSIONNO, dbname);
		goto errxit;
	}
	*nmagicp = (st.st_size / sizeof(**magicp)) - 1;
	(*magicp)++;
	if (needsbyteswap)
		byteswap(*magicp, *nmagicp);
	return 0;

errxit:
	if (fd != -1)
		(void)close(fd);
/*@-branchstate@*/
	if (mm != NULL) {
#ifdef HAVE_MMAP
		(void)munmap(mm, (size_t)st.st_size);
#else
		free(mm);
#endif
	} else {
		*magicp = NULL;
		*nmagicp = 0;
	}
/*@=branchstate@*/
	return -1;
}
/*@=bounds@*/

/*
 * Handle one file.
 */
static int
apprentice_1(fmagic fm, const char *fn, int action)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/
{
/*@-shadow@*/
	struct magic *magic = NULL;
	uint32_t nmagic = 0;
/*@=shadow@*/
	struct mlist *ml;
	int rv = -1;

	if (action == FILE_COMPILE) {
		rv = apprentice_file(fm, &magic, &nmagic, fn, action);
		if (rv)
			return rv;
		return apprentice_compile(fm, &magic, &nmagic, fn, action);
	}
#ifndef COMPILE_ONLY
	if ((rv = apprentice_map(fm, &magic, &nmagic, fn, action)) != 0)
		(void)fprintf(stderr, "%s: Using regular magic file `%s'\n",
		    __progname, fn);
		
	if (rv != 0)
		rv = apprentice_file(fm, &magic, &nmagic, fn, action);

	if (rv != 0)
		return rv;
	     
	if (magic == NULL || nmagic == 0)
		return rv;

	ml = xmalloc(sizeof(*ml));

	ml->magic = magic;
	ml->nmagic = nmagic;

	fm->mlist->prev->next = ml;
	ml->prev = fm->mlist->prev;
/*@-immediatetrans@*/
	ml->next = fm->mlist;
/*@=immediatetrans@*/
/*@-kepttrans@*/
	fm->mlist->prev = ml;
/*@=kepttrans@*/

/*@-compdef -compmempass @*/
	return rv;
/*@=compdef =compmempass @*/
#endif /* COMPILE_ONLY */
}

/* const char *fn: list of magic files */
int
fmagicSetup(fmagic fm, const char *fn, int action)
{
	char *p, *mfn;
	int file_err, errs = -1;

	if (fm->mlist == NULL) {
		static struct mlist mlist;
/*@-immediatetrans@*/
		mlist.next = &mlist;
		mlist.prev = &mlist;
		fm->mlist = &mlist;
/*@=immediatetrans@*/
	}

	mfn = xstrdup(fn);
	fn = mfn;
  
/*@-branchstate@*/
	while (fn != NULL) {
		p = strchr(fn, PATHSEP);
		if (p != NULL)
			*p++ = '\0';
		file_err = apprentice_1(fm, fn, action);
		if (file_err > errs)
			errs = file_err;
		fn = p;
	}
/*@=branchstate@*/
	if (errs == -1)
		(void) fprintf(stderr, "%s: couldn't find any magic files!\n",
		    __progname);
	if (action == FILE_CHECK && errs)
		exit(EXIT_FAILURE);

	free(mfn);
/*@-compdef -compmempass@*/
	return errs;
/*@=compdef =compmempass@*/
}

#ifdef COMPILE_ONLY
int
main(int argc, char *argv[])
	/*@*/
{
	static struct fmagic_s myfmagic;
	fmagic fm = &myfmagic;
	int ret;

	setprogname(argv[0]);       /* Retrofit glibc __progname */

	if (argc != 2) {
		(void)fprintf(stderr, "usage: %s file\n", __progname);
		exit(1);
	}
	fm->magicfile = argv[1];

	exit(apprentice(fm, fm->magicfile, COMPILE));
}
#endif /* COMPILE_ONLY */
