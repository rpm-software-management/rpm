/*
 * print.c - debugging printout routines
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

FILE_RCSID("@(#)Id: print.c,v 1.38 2002/07/03 18:37:44 christos Exp ")

/*@access fmagic @*/

/*@-compmempass@*/
/*@unchecked@*/
static struct fmagic_s myfmagic;
/*@unchecked@*/
fmagic global_fmagic = &myfmagic;
/*@=compmempass@*/

#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

#ifndef COMPILE_ONLY
void
mdump(struct magic *m)
{
	/*@observer@*/
	static const char *typ[] = { "invalid", "byte", "short", "invalid",
				     "long", "string", "date", "beshort",
				     "belong", "bedate", "leshort", "lelong",
				     "ledate", "pstring", "ldate", "beldate",
				     "leldate", "regex" };
	static const char optyp[] = { '@', '&', '|', '^', '+', '-', 
				      '*', '/', '%' };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR) {
		(void) fprintf(stderr, "(%s,",
			       /* Note: type is unsigned */
			       (m->in_type < SZOF(typ)) ? 
					typ[m->in_type] : "*bad*");
		if (m->in_op & OPINVERSE)
			(void) fputc('~', stderr);
		(void) fprintf(stderr, "%c%d),",
			       ((m->in_op&0x7F) < SZOF(optyp)) ? 
					optyp[m->in_op&0x7F] : '?',
				m->in_offset);
	}
	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       /* Note: type is unsigned */
		       (m->type < SZOF(typ)) ? typ[m->type] : "*bad*");
	if (m->mask_op & OPINVERSE)
		(void) fputc('~', stderr);
	if (m->mask) {
		((m->mask_op&0x7F) < SZOF(optyp)) ? 
			(void) fputc(optyp[m->mask_op&0x7F], stderr) :
			(void) fputc('?', stderr);
		if(STRING != m->type || PSTRING != m->type)
			(void) fprintf(stderr, "%.8x", m->mask);
		else {
			if (m->mask & STRING_IGNORE_LOWERCASE) 
				(void) fputc(CHAR_IGNORE_LOWERCASE, stderr);
			if (m->mask & STRING_COMPACT_BLANK) 
				(void) fputc(CHAR_COMPACT_BLANK, stderr);
			if (m->mask & STRING_COMPACT_OPTIONAL_BLANK) 
				(void) fputc(CHAR_COMPACT_OPTIONAL_BLANK,
				stderr);
		}
	}

	(void) fprintf(stderr, ",%c", m->reln);

	if (m->reln != 'x') {
		switch (m->type) {
		case BYTE:
		case SHORT:
		case LONG:
		case LESHORT:
		case LELONG:
		case BESHORT:
		case BELONG:
			(void) fprintf(stderr, "%d", m->value.l);
			break;
		case STRING:
		case PSTRING:
		case REGEX:
			showstr(stderr, m->value.s, -1);
			break;
		case DATE:
		case LEDATE:
		case BEDATE:
			(void)fprintf(stderr, "%s,", fmttime(m->value.l, 1));
			break;
		case LDATE:
		case LELDATE:
		case BELDATE:
			(void)fprintf(stderr, "%s,", fmttime(m->value.l, 0));
			break;
		default:
			(void) fputs("*bad*", stderr);
			break;
		}
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}
#endif

/*
 * ckfputs - fputs, but with error checking
 * ckfprintf - fprintf, but with error checking
 */
void
ckfputs(const char *str, fmagic fm)
{
	FILE *f = (fm->f ? fm->f : stdout);
	if (fputs(str, f) == EOF)
		error(EXIT_FAILURE, 0, "ckfputs write failed.\n");
}

/*VARARGS*/
void
ckfprintf(fmagic fm, const char *fmt, ...)
{
	FILE *f = (fm->f ? fm->f : stdout);

	va_list va;

	va_start(va, fmt);
	(void) vfprintf(f, fmt, va);
	if (ferror(f))
		error(EXIT_FAILURE, 0, "ckfprintf write failed.\n");
	va_end(va);
}

#if !defined(HAVE_ERROR)
/*
 * error - print best error message possible and exit
 */
/*VARARGS*/
void
error(int status, int errnum, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	/* cuz we use stdout for most, stderr here */
	(void) fflush(stdout); 

	if (progname != NULL) 
		(void) fprintf(stderr, "%s: ", progname);
	(void) vfprintf(stderr, fmt, va);
	va_end(va);
#if NOTYET
	if (status)
#endif
		exit(status);
}
#endif

/*VARARGS*/
void
magwarn(const char *f, ...)
{
	fmagic fm = global_fmagic;
	va_list va;

	va_start(va, f);
	/* cuz we use stdout for most, stderr here */
	(void) fflush(stdout); 

	if (progname != NULL) 
		(void) fprintf(stderr, "%s: %s, %d: ", 
			       progname, fm->magicfile, fm->lineno);
	(void) vfprintf(stderr, f, va);
	va_end(va);
	(void) fputc('\n', stderr);
}


#ifndef COMPILE_ONLY
char *
fmttime(long v, int local)
{
	char *pp, *rt;
	time_t t = (time_t)v;
	struct tm *tm;

	if (local) {
		pp = ctime(&t);
	} else {
#ifndef HAVE_DAYLIGHT
		static int daylight = 0;
#ifdef HAVE_TM_ISDST
		static time_t now = (time_t)0;

		if (now == (time_t)0) {
			struct tm *tm1;
			(void)time(&now);
			tm1 = localtime(&now);
			daylight = tm1->tm_isdst;
		}
#endif /* HAVE_TM_ISDST */
#endif /* HAVE_DAYLIGHT */
		if (daylight)
			t += 3600;
		tm = gmtime(&t);
		pp = asctime(tm);
	}

/*@-modobserver@*/
	if ((rt = strchr(pp, '\n')) != NULL)
		*rt = '\0';
/*@=modobserver@*/
	return pp;
}
#endif
