 /*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * file.h - definitions for file(1) program
 * @(#) Id: file.h,v 1.53 2003/03/28 21:02:03 christos Exp 
 */

#ifndef __file_h__
#define __file_h__

/*@-redef@*/

#ifndef HOWMANY
# define HOWMANY 65536		/* how much of the file to look at */
#endif
#define MAXMAGIS 4096		/* max entries in /etc/magic */
#define MAXDESC	64		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

#define MAGICNO		0xF11E041C
#define VERSIONNO	2
#define FILE_MAGICSIZE	(32 * 4)

#define FILE_CHECK	1
#define FILE_COMPILE	2

struct magic {
	/* Word 1 */
	uint16_t cont_level;	/* level of ">" */
	uint8_t nospflag;	/* supress space character */
	uint8_t flag;
#define INDIR	1		/* if '>(...)' appears,  */
#define	UNSIGNED 2		/* comparison is unsigned */
#define OFFADD	4		/* if '>&' appears,  */
	/* Word 2 */
	uint8_t reln;		/* relation (0=eq, '>'=gt, etc) */
	uint8_t vallen;		/* length of string value, if any */
	uint8_t type;		/* int, short, long or string. */
	uint8_t in_type;	/* type of indirrection */
#define 			FILE_BYTE	1
#define				FILE_SHORT	2
#define				FILE_LONG	4
#define				FILE_STRING	5
#define				FILE_DATE	6
#define				FILE_BESHORT	7
#define				FILE_BELONG	8
#define				FILE_BEDATE	9
#define				FILE_LESHORT	10
#define				FILE_LELONG	11
#define				FILE_LEDATE	12
#define				FILE_PSTRING	13
#define				FILE_LDATE	14
#define				FILE_BELDATE	15
#define				FILE_LELDATE	16
#define				FILE_REGEX	17
	/* Word 3 */
	uint8_t in_op;		/* operator for indirection */
	uint8_t mask_op;	/* operator for mask */
	uint8_t dummy1;
	uint8_t dummy2;
#define				FILE_OPS	"&|^+-*%/"
#define				FILE_OPAND	0
#define				FILE_OPOR	1
#define				FILE_OPXOR	2
#define				FILE_OPADD	3
#define				FILE_OPMINUS	4
#define				FILE_OPMULTIPLY	5
#define				FILE_OPDIVIDE	6
#define				FILE_OPMODULO	7
#define				FILE_OPINVERSE	0x80
	/* Word 4 */
	int32_t offset;		/* offset to magic number */
	/* Word 5 */
	int32_t in_offset;	/* offset from indirection */
	/* Word 6 */
	uint32_t mask;	/* mask before comparison with value */
	/* Word 7 */
	uint32_t dummy3;
	/* Word 8 */
	uint32_t dummp4;
	/* Words 9-16 */
	union VALUETYPE {
		uint8_t b;
		uint16_t h;
		uint32_t l;
		char s[MAXstring];
		const char * buf;
		uint8_t hs[2];	/* 2 bytes of a fixed-endian "short" */
		uint8_t hl[4];	/* 4 bytes of a fixed-endian "long" */
	} value;		/* either number or string */
	/* Words 17..31 */
	char desc[MAXDESC];	/* description */
};

#define BIT(A)   (1 << (A))
#define STRING_IGNORE_LOWERCASE		BIT(0)
#define STRING_COMPACT_BLANK		BIT(1)
#define STRING_COMPACT_OPTIONAL_BLANK	BIT(2)
#define CHAR_IGNORE_LOWERCASE		'c'
#define CHAR_COMPACT_BLANK		'B'
#define CHAR_COMPACT_OPTIONAL_BLANK	'b'


/* list of magic entries */
struct mlist {
	struct magic *magic;		/* array of magic entries */
	uint32_t nmagic;		/* number of entries in array */
	int mapped;  /* allocation type: 0 => apprentice_file
		      *                  1 => apprentice_map + malloc
		      *                  2 => apprentice_map + mmap */
/*@relnull@*/
	struct mlist *next;
/*@relnull@*/
	struct mlist *prev;
};

struct magic_set {
    struct mlist *mlist;
    struct cont {
	size_t len;
	int32_t *off;
    } c;
    struct out {
	char *buf;
	char *ptr;
	size_t len;
	size_t size;
    } o;
    int flags;
    int haderr;
};

enum fmagicFlags_e {
/*@-enummemuse@*/
    FMAGIC_FLAGS_NONE		= 0,
/*@=enummemuse@*/
    FMAGIC_FLAGS_DEBUG		= (1 << 0),
    FMAGIC_FLAGS_BRIEF		= (1 << 1),	/*!< brief output format */
    FMAGIC_FLAGS_MIME		= (1 << 2),	/*!< output as mime-types */
    FMAGIC_FLAGS_CONTINUE	= (1 << 3),	/*!< continue after 1st match */
    FMAGIC_FLAGS_FOLLOW		= (1 << 4),	/*!< follow symlinks? */
    FMAGIC_FLAGS_SPECIAL	= (1 << 5),	/*!< analyze block devices? */
    FMAGIC_FLAGS_UNCOMPRESS	= (1 << 6),	/*!< uncompress files? */
    FMAGIC_FLAGS_NOPAD		= (1 << 7)	/*!< don't pad output */
};

struct fmagic_s {
    int flags;			/*!< bit(s) to control fmagic behavior. */
/*@dependent@*/ /*@observer@*/ /*@relnull@*/
    const char *magicfile;	/*!< name of the magic file		*/
/*@dependent@*/ /*@observer@*/
    const char *separator;	/*!< file name/type separator (default ":" */
    int lineno;			/*!< current line number in magic file	*/
/*@relnull@*/
    struct mlist * mlist;	/*!< list of arrays of magic entries	*/
/*@relnull@*/
    struct mlist * ml;		/*!< current magic array item		*/
/*@observer@*/
    const char * fn;		/*!< current file name			*/
    int fd;			/*!< current file descriptor            */
    struct stat sb;		/*!< current file stat(2) buffer	*/
/*@relnull@*/
    unsigned char * buf;	/*!< current file buffer		*/
    int nb;			/*!< current no. bytes in file buffer	*/
    union VALUETYPE val;	/*!< current magic expression value	*/
    int cls;			/*!< Elf class				*/
    int swap;			/*!< Elf swap bytes?			*/
/*@dependent@*/
    char * obp;			/*!< current output buffer pointer	*/
    size_t nob;			/*!< bytes remaining in output buffer	*/
    char obuf[512];		/*!< output buffer			*/
};

typedef /*@abstract@*/ struct fmagic_s * fmagic;

/*@unchecked@*/
extern fmagic global_fmagic;

/*@unchecked@*//*@observer@*/
extern const char * default_magicfile;

#ifdef __cplusplus
extern "C" {
#endif

/*@mayexit@*/
extern int fmagicSetup(fmagic fm, const char *fn, int action)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;
extern int fmagicProcess(fmagic fm, const char *fn, int wid)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;

extern int fmagicA(fmagic fm)
	/*@modifies fm @*/;
extern int fmagicD(fmagic fm)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;
extern void fmagicE(fmagic fm)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;
extern int fmagicF(fmagic fm, int zfl)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;
extern int fmagicS(fmagic fm)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/;
extern int fmagicZ(fmagic fm)
	/*@globals fileSystem, internalState @*/
	/*@modifies fm, fileSystem, internalState @*/;

extern void file_printf(const fmagic fm, const char *f, ...)
	/*@modifies fm @*/;

/*@observer@*/
extern const char *file_fmttime(uint32_t v, int local)
	/*@*/;

extern void file_magwarn(const char *f, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void file_mdump(struct magic *m)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern void file_showstr(FILE *fp, const char *s, size_t len)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/;

extern uint32_t file_signextend(struct magic *m, uint32_t v)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
extern int file_pipe2file(int fd, const void *startbuf, size_t nbytes)
	/*@globals errno, fileSystem, internalState @*/
	/*@modifies errno, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

/*@=redef@*/
#endif /* __file_h__ */
