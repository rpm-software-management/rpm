#ifndef H_RPMXP
#define H_RPMXP
                                                                                
/** \ingroup rpmdep rpmtrans
 * \file tools/rpmxp.h
 * Structure(s) used for an xml parser.
 */

#include <libxml/xmlreader.h>

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmxp_debug;
/*@=exportlocal@*/

/**
 */
typedef struct rpmxp_s * rpmxp;


#if defined(_RPMXP_INTERNAL)
/**
 * An xml parser.
 */
struct rpmxp_s {
    xmlTextReaderPtr reader;

    xmlChar * name;
    xmlChar * value;
    int depth;
    int nodeType;
    int isEmptyElement;

    int n;
};
#endif

/**
* Destroy an xml parser.
 * @param xp		rpm xml parser
 * @return		NULL always
 */
rpmxp rpmxpFree(/*@only@*/ /*@null@*/ rpmxp xp)
	/*@modifies xp @*/;

/**
 * Create an xml parser.
 * @param fn		xml file name
 * @return		new xml parser
 */
/*@only@*/
rpmxp rpmxpNew(const char * fn)
	/*@*/;

/**
 * Read next item from an xml parser.
 * @param xp		rpm xml parser
 * @return		1 to continue
 */
int rpmxpRead(rpmxp xp)
	/*@modifies xp @*/;

/**
 * Parse all elements from an xml parser.
 * @param xp		rpm xml parser
 * @return		0 on success
 */
int rpmxpProcess(rpmxp xp)
	/*@modifies xp @*/;

/**
 * Parse all elements from an xml parser.
 * @param xp		rpm xml parser
 * @return		0 on success
 */
int rpmxpParseFile(rpmxp xp)
	/*@modifies xp @*/;

#endif	/* H_RPMXP */
