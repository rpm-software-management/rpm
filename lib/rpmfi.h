#ifndef H_RPMFI
#define H_RPMFI

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmfi.h
 * Structure used for file info tag sets.
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
    int i;			/*!< File index. */

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
/*@only@*/ /*?null?*/
    const char ** flangs;	/*!< File lang(s) */

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
    const char ** fuser;	/*!< File owner(s) */
/*@only@*/ /*@null@*/
    const char ** fgroup;	/*!< File group(s) */
/*@only@*/ /*@null@*/
    uid_t * fuids;		/*!< File uid(s) */
/*@only@*/ /*@null@*/
    gid_t * fgids;		/*!< File gid(s) */

/*@only@*/ /*@null@*/
    char * fstates;		/*!< File state(s) (from header) */

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
    int bnlmax;			/*!< Length (in bytes) of longest basename. */
    int dnlmax;			/*!< Length (in bytes) of longest dirname. */
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
 * Retrieve key from transaction element file info
 * @param fi		transaction element file info
 * @return		transaction element file info key
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
fnpyKey rpmfiGetKey(TFI_t fi)
	/*@*/;

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
 * Return next file info set iterator index.
 * @param fi		file info set
 * @return		file info set iterator index, -1 on termination
 */
int tfiNext(/*@null@*/ TFI_t fi)
	/*@modifies fi @*/;

/**
 * Initialize file info set iterator.
 * @param fi		file info set
 * @param ix		file info set index
 * @return		file info set, NULL if ix is out of range
 */
/*@null@*/
TFI_t tfiInit(/*@null@*/ TFI_t fi, int ix)
	/*@modifies fi @*/;

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
