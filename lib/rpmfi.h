#ifndef H_RPMFI
#define H_RPMFI

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmfi.h
 * Structure(s) used for file info tag sets.
 */

/**
 */
typedef struct sharedFileInfo_s *		sharedFileInfo;

/**
 */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherFileNum;
    int otherPkg;
    int isRemoved;
};

/**
 * A package filename set.
 */
struct TFI_s {
    int i;			/*!< Current file index. */
    int j;			/*!< Current directory index. */

/*@observer@*/
    const char * Type;		/*!< Tag name. */

    rpmTag tagN;		/*!< Header tag. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for file set (or NULL) */

/*@only@*/ /*?null?*/
    const char ** bnl;		/*!< Base name(s) (from header) */
/*@only@*/ /*?null?*/
    const char ** dnl;		/*!< Directory name(s) (from header) */

/*@only@*/ /*?null?*/
    const char ** fmd5s;	/*!< File MD5 sum(s) (from header) */
/*@only@*/ /*?null?*/
    const char ** flinks;	/*!< File link(s) (from header) */
/*@only@*/ /*@null@*/
    const char ** flangs;	/*!< File lang(s) (from header) */

/*@only@*/ /*?null?*/
          uint_32 * dil;	/*!< Directory indice(s) (from header) */
/*@only@*/ /*?null?*/
    const uint_32 * fflags;	/*!< File flag(s) (from header) */
/*@only@*/ /*?null?*/
    const uint_32 * fsizes;	/*!< File size(s) (from header) */
/*@only@*/ /*?null?*/
    const uint_32 * fmtimes;	/*!< File modification time(s) (from header) */
/*@only@*/ /*?null?*/
          uint_16 * fmodes;	/*!< File mode(s) (from header) */
/*@only@*/ /*?null?*/
    const uint_16 * frdevs;	/*!< File rdev(s) (from header) */

/*@only@*/ /*@null@*/
    const char ** fuser;	/*!< File owner(s) (from header) */
/*@only@*/ /*@null@*/
    const char ** fgroup;	/*!< File group(s) (from header) */
/*@only@*/ /*@null@*/
    uid_t * fuids;		/*!< File uid(s) */
/*@only@*/ /*@null@*/
    gid_t * fgids;		/*!< File gid(s) */

/*@only@*/ /*@null@*/
    char * fstates;		/*!< File state(s) (from header) */

/*@only@*/ /*?null?*/
    const uint_32 * vflags;	/*!< File verify flag(s) (from header) */

    int_32 dc;			/*!< No. of directories. */
    int_32 fc;			/*!< No. of files. */

/*=============================*/
/*@dependent@*/
    transactionElement te;

    HGE_t hge;			/*!< Vector to headerGetEntry() */
    HAE_t hae;			/*!< Vector to headerAddEntry() */
    HME_t hme;			/*!< Vector to headerModifyEntry() */
    HRE_t hre;			/*!< Vector to headerRemoveEntry() */
    HFD_t hfd;			/*!< Vector to headerFreeData() */
/*-----------------------------*/
    uid_t uid;			/*!< File uid (default). */
    gid_t gid;			/*!< File gid (default). */
    uint_32 flags;		/*!< File flags (default). */
    fileAction action;		/*!< File disposition (default). */
/*@owned@*/
    fileAction * actions;	/*!< File disposition(s). */
/*@owned@*/
    struct fingerPrint_s * fps;	/*!< File fingerprint(s). */
/*@owned@*/
    const char ** obnl;		/*!< Original basename(s) (from header) */
/*@owned@*/
    const char ** odnl;		/*!< Original dirname(s) (from header) */
/*@unused@*/
    int_32 * odil;		/*!< Original dirindex(s) (from header) */

/*@only@*/ /*@null@*/
    unsigned char * md5s;	/*!< File md5 sums in binary. */

/*@only@*/ /*@null@*/
    char * fn;			/*!< File name buffer. */
    int fnlen;			/*!< FIle name buffer length. */

    int astriplen;
    int striplen;
    unsigned int archiveSize;
    mode_t dperms;		/*!< Directory perms (0755) if not mapped. */
    mode_t fperms;		/*!< File perms (0644) if not mapped. */
/*@only@*/ /*@null@*/
    const char ** apath;
    int mapflags;
/*@owned@*/ /*@null@*/
    int * fmapflags;
/*@owned@*/
    FSM_t fsm;			/*!< File state machine data. */
    int keep_header;		/*!< Keep header? */
/*@owned@*/
    sharedFileInfo replaced;	/*!< (TR_ADDED) */
/*@owned@*/
    uint_32 * replacedSizes;	/*!< (TR_ADDED) */
    unsigned int record;	/*!< (TR_REMOVED) */
    int magic;
#define	TFIMAGIC	0x09697923
/*=============================*/

/*@refs@*/ int nrefs;		/*!< Reference count. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a file info set instance.
 * @param fi		file info set
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
TFI_t rpmfiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ TFI_t fi,
		/*@null@*/ const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
TFI_t XrpmfiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ TFI_t fi,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies fi @*/;
/*@=exportlocal@*/
#define	rpmfiUnlink(_fi, _msg) XrpmfiUnlink(_fi, _msg, __FILE__, __LINE__)

/**
 * Reference a file info set instance.
 * @param fi		file info set
 * @return		new file info set reference
 */
/*@unused@*/
TFI_t rpmfiLink (/*@null@*/ TFI_t fi, /*@null@*/ const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
TFI_t XrpmfiLink (/*@null@*/ TFI_t fi, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies fi @*/;
#define	rpmfiLink(_fi, _msg)	XrpmfiLink(_fi, _msg, __FILE__, __LINE__)

/**
 * Retrieve key from transaction element file info.
 * @param fi		transaction element file info
 * @return		transaction element file info key
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
fnpyKey rpmfiGetKey(TFI_t fi)
	/*@*/;

/**
 * Return file count from file info set.
 * @param fi		file info set
 * @return		current file count
 */
int tfiGetFC(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file index from file info set.
 * @param fi		file info set
 * @return		current file index
 */
/*@unused@*/
int tfiGetFX(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Set current file index in file info set.
 * @param fi		file info set
 * @param fx		new file index
 * @return		current file index
 */
/*@unused@*/
int tfiSetFX(/*@null@*/ TFI_t fi, int fx)
	/*@modifies fi @*/;

/**
 * Return directory count from file info set.
 * @param fi		file info set
 * @return		current directory count
 */
int tfiGetDC(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current directory index from file info set.
 * @param fi		file info set
 * @return		current directory index
 */
int tfiGetDX(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Set current directory index in file info set.
 * @param fi		file info set
 * @param fx		new directory index
 * @return		current directory index
 */
int tfiSetDX(/*@null@*/ TFI_t fi, int dx)
	/*@modifies fi @*/;

/**
 * Return current base name from file info set.
 * @param fi		file info set
 * @return		current base name, NULL on invalid
 */
/*@null@*/
const char * tfiGetBN(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current directory name from file info set.
 * @param fi		file info set
 * @return		current directory, NULL on invalid
 */
/*@null@*/
const char * tfiGetDN(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file name from file info set.
 * @param fi		file info set
 * @return		current file name
 */
/*@observer@*/
const char * tfiGetFN(/*@null@*/ TFI_t fi)
	/*@modifies fi @*/;

/**
 * Return current file flags from file info set.
 * @param fi		file info set
 * @return		current file flags, 0 on invalid
 */
int_32 tfiGetFFlags(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file verify flags from file info set.
 * @param fi		file info set
 * @return		current file verify flags, 0 on invalid
 */
int_32 tfiGetVFlags(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file mode from file info set.
 * @param fi		file info set
 * @return		current file mode, 0 on invalid
 */
int_16 tfiGetFMode(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file state from file info set.
 * @param fi		file info set
 * @return		current file state, 0 on invalid
 */
rpmfileState tfiGetFState(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file (binary) md5 digest from file info set.
 * @param fi		file info set
 * @return		current file md5 digest, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const unsigned char * tfiGetMD5(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file linkto (i.e. symlink(2) target) from file info set.
 * @param fi		file info set
 * @return		current file linkto, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * tfiGetFLink(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file size from file info set.
 * @param fi		file info set
 * @return		current file size, 0 on invalid
 */
int_32 tfiGetFSize(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file rdev from file info set.
 * @param fi		file info set
 * @return		current file rdev, 0 on invalid
 */
int_16 tfiGetFRdev(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file modify time from file info set.
 * @param fi		file info set
 * @return		current file modify time, 0 on invalid
 */
int_32 tfiGetFMtime(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file owner from file info set.
 * @param fi		file info set
 * @return		current file owner, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * tfiGetFUser(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return current file group from file info set.
 * @param fi		file info set
 * @return		current file group, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * tfiGetFGroup(/*@null@*/ TFI_t fi)
	/*@*/;

/**
 * Return next file iterator index.
 * @param fi		file info set
 * @return		file iterator index, -1 on termination
 */
int tfiNext(/*@null@*/ TFI_t fi)
	/*@modifies fi @*/;

/**
 * Initialize file iterator index.
 * @param fi		file info set
 * @param fx		file iterator index
 * @return		file info set
 */
/*@null@*/
TFI_t tfiInit(/*@null@*/ TFI_t fi, int fx)
	/*@modifies fi @*/;

/**
 * Return next directory iterator index.
 * @param fi		file info set
 * @return		directory iterator index, -1 on termination
 */
/*@unused@*/
int tdiNext(/*@null@*/ TFI_t fi)
	/*@modifies fi @*/;

/**
 * Initialize directory iterator index.
 * @param fi		file info set
 * @param dx		directory iterator index
 * @return		file info set, NULL if dx is out of range
 */
/*@unused@*/ /*@null@*/
TFI_t tdiInit(/*@null@*/ TFI_t fi, int dx)
	/*@modifies fi @*/;

/**
 * Destroy a file info set.
 * @param fi		file set
 * @param freefimem	free fi memory too?
 * @return		NULL always
 */
/*@null@*/
TFI_t fiFree(/*@killref@*/ /*@only@*/ /*@null@*/ TFI_t fi, int freefimem)
	/*@modifies fi@*/;

/**
 * Create and load a file info set.
 * @param ts		transaction set
 * @param fi		file set (NULL if creating)
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new file set
 */
/*@null@*/
TFI_t fiNew(rpmTransactionSet ts, /*@null@*/ TFI_t fi,
		Header h, rpmTag tagN, int scareMem)
	/*@modifies ts, fi, h @*/;

/**
 * Return file type from mode_t.
 * @param mode		file mode bits (from header)
 * @return		file type
 */
fileTypes whatis(uint_16 mode)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */
