#ifndef _H_RPMFC_
#define _H_RPMFC_

typedef struct rpmfc_s * rpmfc;

struct rpmfc_s {
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
#define	RPMFC_ELF	(RPMFC_ELF32|RPMFC_ELF64)

    RPMFC_EXECUTABLE		= (1 <<  8),
    RPMFC_SCRIPT		= (1 <<  9),
    RPMFC_TEXT			= (1 << 10),
    RPMFC_DATA			= (1 << 11),	/* XXX unused */
    RPMFC_DOCUMENT		= (1 << 12),
    RPMFC_STATIC		= (1 << 13),
    RPMFC_NOTSTRIPPED		= (1 << 14),
    RPMFC_COMPRESSED		= (1 << 15),

    RPMFC_DIRECTORY		= (1 << 16),
    RPMFC_SYMLINK		= (1 << 17),
    RPMFC_DEVICE		= (1 << 18),
    RPMFC_LIBRARY		= (1 << 19),
    RPMFC_ARCHIVE		= (1 << 20),
    RPMFC_FONT			= (1 << 21),
    RPMFC_IMAGE			= (1 << 22),
    RPMFC_MANPAGE		= (1 << 23),

    RPMFC_WHITE			= (1 << 29),
    RPMFC_INCLUDE		= (1 << 30),
    RPMFC_ERROR			= (1 << 31)
};
typedef	enum FCOLOR_e FCOLOR_t;

struct rpmfcTokens_s {
/*@observer@*/
    const char * token;
    int colors;
};

typedef struct rpmfcTokens_s * rpmfcToken;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
int rpmfcColoring(const char * fmstr)
	/*@*/;

/**
 */
void rpmfcPrint(const char * msg, rpmfc fc, FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;
/**
 */
/*@null@*/
rpmfc rpmfcFree(/*@only@*/ /*@null@*/ rpmfc fc)
	/*@modifies fc @*/;

/**
 */
rpmfc rpmfcNew(void)
	/*@*/;

/**
 */
int rpmfcClassify(/*@out@*/ rpmfc *fcp, ARGV_t argv)
	/*@modifies *fcp @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_RPMFC_ */
