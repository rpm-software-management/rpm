/*
 * Header file for public domain tar (tape archive) program.
 *
 * @(#)tar.h 1.20 86/10/29	Public Domain.
 *
 * Created 25 August 1985 by John Gilmore, ihnp4!hoptoad!gnu.
 *
 * Id: tar.h,v 1.5 1999/01/13 15:44:10 christos Exp  # checkin only
 */

#ifndef	__tar_h__
#define	__tar_h__

/*
 * Header block on tape.
 *
 * I'm going to use traditional DP naming conventions here.
 * A "block" is a big chunk of stuff that we do I/O on.
 * A "record" is a piece of info that we care about.
 * Typically many "record"s fit into a "block".
 */
#define	RECORDSIZE	512
#define	NAMSIZ	100
#define	TUNMLEN	32
#define	TGNMLEN	32

/*@-fielduse@*/
union record {
	char		charptr[RECORDSIZE];
	struct header {
		char	name[NAMSIZ];
		char	mode[8];
		char	uid[8];
		char	gid[8];
		char	size[12];
		char	mtime[12];
		char	chksum[8];
		char	linkflag;
		char	linkname[NAMSIZ];
		char	magic[8];
		char	uname[TUNMLEN];
		char	gname[TGNMLEN];
		char	devmajor[8];
		char	devminor[8];
	} header;
};
/*@=fielduse@*/

/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS	"        "	/* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
#define	TMAGIC		"ustar  "	/* 7 chars and a null */

#if 0
/* The linkflag defines the type of file */
#define	LF_OLDNORMAL	'\0'		/* Normal disk file, Unix compat */
#define	LF_NORMAL	'0'		/* Normal disk file */
#define	LF_LINK		'1'		/* Link to previously dumped file */
#define	LF_SYMLINK	'2'		/* Symbolic link */
#define	LF_CHR		'3'		/* Character special file */
#define	LF_BLK		'4'		/* Block special file */
#define	LF_DIR		'5'		/* Directory */
#define	LF_FIFO		'6'		/* FIFO special file */
#define	LF_CONTIG	'7'		/* Contiguous file */
/* Further link types may be defined later. */

/*
 * Exit codes from the "tar" program
 */
#define	EX_SUCCESS	0		/* success! */
#define	EX_ARGSBAD	1		/* invalid args */
#define	EX_BADFILE	2		/* invalid filename */
#define	EX_BADARCH	3		/* bad archive */
#define	EX_SYSTEM	4		/* system gave unexpected error */


/*
 * Global variables
 */
extern union record	*ar_block;	/* Start of block of archive */
extern union record	*ar_record;	/* Current record of archive */
extern union record	*ar_last;	/* Last+1 record of archive block */
extern char		ar_reading;	/* 0 writing, !0 reading archive */
extern int		blocking;	/* Size of each block, in records */
extern int		blocksize;	/* Size of each block, in bytes */
extern char		*ar_file;	/* File containing archive */
extern char		*name_file;	/* File containing names to work on */
extern char		*tar;		/* Name of this program */

/*
 * Flags from the command line
 */
extern char	f_reblock;		/* -B */
extern char	f_create;		/* -c */
extern char	f_debug;		/* -d */
extern char	f_sayblock;		/* -D */
extern char	f_follow_links;		/* -h */
extern char	f_ignorez;		/* -i */
extern char	f_keep;			/* -k */
extern char	f_modified;		/* -m */
extern char	f_oldarch;		/* -o */
extern char	f_use_protection;	/* -p */
extern char	f_sorted_names;		/* -s */
extern char	f_list;			/* -t */
extern char	f_namefile;		/* -T */
extern char	f_verbose;		/* -v */
extern char	f_extract;		/* -x */
extern char	f_compress;		/* -z */

/*
 * We now default to Unix Standard format rather than 4.2BSD tar format.
 * The code can actually produce all three:
 *	f_standard	ANSI standard
 *	f_oldarch	V7
 *	neither		4.2BSD
 * but we don't bother, since 4.2BSD can read ANSI standard format anyway.
 * The only advantage to the "neither" option is that we can cmp(1) our
 * output to the output of 4.2BSD tar, for debugging.
 */
#define		f_standard		(!f_oldarch)

/*
 * Structure for keeping track of filenames and lists thereof.
 */
struct name {
	struct name	*next;
	short		length;
	char		found;
	char		name[NAMSIZ+1];
};

extern struct name	*namelist;	/* Points to first name in list */
extern struct name	*namelast;	/* Points to last name in list */

extern int		archive;	/* File descriptor for archive file */
extern int		errors;		/* # of files in error */

/*
 *
 * Due to the next struct declaration, each routine that includes
 * "tar.h" must also include <sys/types.h>.  I tried to make it automatic,
 * but System V has no defines in <sys/types.h>, so there is no way of
 * knowing when it has been included.  In addition, it cannot be included
 * twice, but must be included exactly once.  Argghh!
 *
 * Thanks, typedef.  Thanks, USG.
 */
struct link {
	struct link	*next;
	dev_t		dev;
	ino_t		ino;
	short		linkcount;
	char		name[NAMSIZ+1];
};

extern struct link	*linklist;	/* Points to first link in list */

/*
 * Error recovery stuff
 */
extern char		read_error_flag;
#endif

#endif	/* __tar_h__ */
