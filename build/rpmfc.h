#ifndef _H_RPMFC_
#define _H_RPMFC_

typedef struct fclass_s * FCLASS_t;

struct fclass_s {
    ARGV_t av;		/*!< file(1) output lines */
    int ac;		/*!< no. of lines */
    int ix;		/*!< current lineno */
    ARGV_t fn;		/*!< file names */
    ARGI_t fcolor;	/*!< file colors */
    ARGI_t fdictx;	/*!< file class dictionary indices */
    ARGV_t dict;	/*!< file class dictionary */
};


enum FCOLOR_e {
    RPMFC_BLACK			= 0,
    RPMFC_ELF32			= (1 <<  0),
    RPMFC_ELF64			= (1 <<  1),

    RPMFC_EXECUTABLE		= (1 <<  2),
    RPMFC_SCRIPT		= (1 <<  3),
    RPMFC_TEXT			= (1 <<  4),
    RPMFC_DOCUMENT		= (1 <<  5),

    RPMFC_DIRECTORY		= (1 <<  8),
    RPMFC_SYMLINK		= (1 <<  9),
    RPMFC_DEVICE		= (1 << 10),

    RPMFC_STATIC		= (1 << 16),
    RPMFC_NOTSTRIPPED		= (1 << 17),
    RPMFC_COMPRESSED		= (1 << 18),

    RPMFC_LIBRARY		= (1 << 24),
    RPMFC_ARCHIVE		= (1 << 25),
    RPMFC_FONT			= (1 << 26),
    RPMFC_IMAGE			= (1 << 27),
    RPMFC_MANPAGE		= (1 << 28),

    RPMFC_WHITE			= (1 << 29),
    RPMFC_INCLUDE		= (1 << 30),
    RPMFC_ERROR			= (1 << 31)
};
typedef	enum FCOLOR_e FCOLOR_t;

struct fclassTokens_s {
/*@observer@*/
    const char * token;
    int colors;
};

typedef struct fclassTokens_s * fclassToken;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
int fclassColoring(const char * fmstr)
	/*@*/;

/**
 */
void fclassPrint(const char * msg, FCLASS_t fc, FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;
/**
 */
/*@null@*/
FCLASS_t fclassFree(/*@only@*/ /*@null@*/ FCLASS_t fc)
	/*@modifies fc @*/;

/**
 */
FCLASS_t fclassNew(void)
	/*@*/;

/**
 */
int fclassClassify(/*@out@*/ FCLASS_t *fcp, ARGV_t argv)
	/*@modifies *fcp @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMFC_ */
