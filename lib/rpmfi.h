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
struct rpmfi_s {
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
    rpmte te;

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

/*@only@*/
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
#define	RPMFIMAGIC	0x09697923
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
rpmfi rpmfiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmfi fi,
		/*@null@*/ const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmfi XrpmfiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmfi fi,
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
rpmfi rpmfiLink (/*@null@*/ rpmfi fi, /*@null@*/ const char * msg)
	/*@modifies fi @*/;

/** @todo Remove debugging entry from the ABI. */
rpmfi XrpmfiLink (/*@null@*/ rpmfi fi, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies fi @*/;
#define	rpmfiLink(_fi, _msg)	XrpmfiLink(_fi, _msg, __FILE__, __LINE__)

/**
 * Retrieve key from transaction element file info.
 * @param fi		transaction element file info
 * @return		transaction element file info key
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
fnpyKey rpmfiKey(rpmfi fi)
	/*@*/;

/**
 * Return file count from file info set.
 * @param fi		file info set
 * @return		current file count
 */
int rpmfiFC(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file index from file info set.
 * @param fi		file info set
 * @return		current file index
 */
/*@unused@*/
int rpmfiFX(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Set current file index in file info set.
 * @param fi		file info set
 * @param fx		new file index
 * @return		current file index
 */
/*@unused@*/
int rpmfiSetFX(/*@null@*/ rpmfi fi, int fx)
	/*@modifies fi @*/;

/**
 * Return directory count from file info set.
 * @param fi		file info set
 * @return		current directory count
 */
int rpmfiDC(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current directory index from file info set.
 * @param fi		file info set
 * @return		current directory index
 */
int rpmfiDX(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Set current directory index in file info set.
 * @param fi		file info set
 * @param fx		new directory index
 * @return		current directory index
 */
int rpmfiSetDX(/*@null@*/ rpmfi fi, int dx)
	/*@modifies fi @*/;

/**
 * Return current base name from file info set.
 * @param fi		file info set
 * @return		current base name, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * rpmfiBN(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current directory name from file info set.
 * @param fi		file info set
 * @return		current directory, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * rpmfiDN(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file name from file info set.
 * @param fi		file info set
 * @return		current file name
 */
/*@observer@*/
const char * rpmfiFN(/*@null@*/ rpmfi fi)
	/*@modifies fi @*/;

/**
 * Return current file flags from file info set.
 * @param fi		file info set
 * @return		current file flags, 0 on invalid
 */
int_32 rpmfiFFlags(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file verify flags from file info set.
 * @param fi		file info set
 * @return		current file verify flags, 0 on invalid
 */
int_32 rpmfiVFlags(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file mode from file info set.
 * @param fi		file info set
 * @return		current file mode, 0 on invalid
 */
int_16 rpmfiFMode(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file state from file info set.
 * @param fi		file info set
 * @return		current file state, 0 on invalid
 */
rpmfileState rpmfiFState(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file (binary) md5 digest from file info set.
 * @param fi		file info set
 * @return		current file md5 digest, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const unsigned char * rpmfiMD5(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file linkto (i.e. symlink(2) target) from file info set.
 * @param fi		file info set
 * @return		current file linkto, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * rpmfiFLink(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file size from file info set.
 * @param fi		file info set
 * @return		current file size, 0 on invalid
 */
int_32 rpmfiFSize(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file rdev from file info set.
 * @param fi		file info set
 * @return		current file rdev, 0 on invalid
 */
int_16 rpmfiFRdev(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file modify time from file info set.
 * @param fi		file info set
 * @return		current file modify time, 0 on invalid
 */
int_32 rpmfiFMtime(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file owner from file info set.
 * @param fi		file info set
 * @return		current file owner, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * rpmfiFUser(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return current file group from file info set.
 * @param fi		file info set
 * @return		current file group, NULL on invalid
 */
/*@observer@*/ /*@null@*/
const char * rpmfiFGroup(/*@null@*/ rpmfi fi)
	/*@*/;

/**
 * Return next file iterator index.
 * @param fi		file info set
 * @return		file iterator index, -1 on termination
 */
int rpmfiNext(/*@null@*/ rpmfi fi)
	/*@modifies fi @*/;

/**
 * Initialize file iterator index.
 * @param fi		file info set
 * @param fx		file iterator index
 * @return		file info set
 */
/*@null@*/
rpmfi rpmfiInit(/*@null@*/ rpmfi fi, int fx)
	/*@modifies fi @*/;

/**
 * Return next directory iterator index.
 * @param fi		file info set
 * @return		directory iterator index, -1 on termination
 */
/*@unused@*/
int rpmfiNextD(/*@null@*/ rpmfi fi)
	/*@modifies fi @*/;

/**
 * Initialize directory iterator index.
 * @param fi		file info set
 * @param dx		directory iterator index
 * @return		file info set, NULL if dx is out of range
 */
/*@unused@*/ /*@null@*/
rpmfi rpmfiInitD(/*@null@*/ rpmfi fi, int dx)
	/*@modifies fi @*/;

/**
 * Destroy a file info set.
 * @param fi		file set
 * @param freefimem	free fi memory too?
 * @return		NULL always
 */
/*@null@*/
rpmfi rpmfiFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmfi fi, int freefimem)
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
rpmfi rpmfiNew(rpmts ts, /*@null@*/ rpmfi fi,
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
