/* SEXP implementation code sexp-basic.c
 * List-based implementation of S-expressions.
 * Ron Rivest
 * 5/5/1997
 */

#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sexp.h"

/*@access sexpIter @*/
/*@access sexpSimpleString @*/

/*@access sexpList @*/
/*@access sexpString @*/
/*@access sexpObject @*/

/******************/
/* ERROR MESSAGES */
/******************/

/* ErrorMessage(level,msg,c1,c2)
 * prints error message on standard output (msg is a c format string)
 * c1 and c2 are (optional) integer parameters for the message.
 * Terminates iff level==ERROR, otherwise returns.
 */
void ErrorMessage(int level, const char *fmt, ...)
{
    va_list ap;

    (void) fflush(stdout);
    printf("\n*** ");
    if (level==WARNING) printf("Warning: ");
    else if (level==ERROR) printf("Error: ");
    va_start(ap, fmt);
    (void) vprintf(fmt, ap);
    va_end(ap);
    printf(" ***\n");
    if (level==ERROR) exit(EXIT_FAILURE);
}

/**********************/
/* STORAGE ALLOCATION */
/**********************/

/* initializeMemory()
 * take care of memory initialization
 */
void initializeMemory(void)
{ ; } /* nothing in this implementation -- use malloc */

/* sexpAlloc(n)
 * Allocates n bytes of storage.
 * Terminates execution if no memory available.
 */
void * sexpAlloc(size_t n)
{
    void * c = calloc(1, n);

    if (c == NULL) ErrorMessage(ERROR,"Error in sexpAlloc: out of memory!");
/*@-nullret@*/
    return c;
/*@=nullret@*/
}

/***********************************/
/* SEXP SIMPLE STRING MANIPULATION */
/***********************************/

/* newSimpleString()
 * Creates and initializes new sexpSimpleString object.
 * Allocates 16-character buffer to hold string.
 */
sexpSimpleString newSimpleString(void)
{
    sexpSimpleString ss = (sexpSimpleString) sexpAlloc(sizeof(*ss));
    ss->length = 0;
    ss->allocatedLength = 16;
    ss->string = sexpAlloc(16);
    return ss;
}
			
/* simpleStringLength(ss)
 * returns length of simple string
 */
long int simpleStringLength(sexpSimpleString ss)
{
    return ss->length;
}

/* simpleStringString(ss)
 * returns pointer to character array of simple string
 */
octet *simpleStringString(sexpSimpleString ss)
{
    return ss->string;
}

/* reallocateSimpleString(ss)
 * Changes space allocated to ss.
 * Space allocated is set to roughly 3/2 the current string length, plus 16.
 */
sexpSimpleString reallocateSimpleString(sexpSimpleString ss)
{
    if (ss == NULL) {
	ss = newSimpleString();
	if (ss == NULL) return NULL;
    }
    if (ss->string == NULL)
	ss->string = (octet *)sexpAlloc(16);
    else {
	size_t newsize = 16 + 3*(ss->length)/2;
	octet * newstring = sexpAlloc(newsize);

	memcpy(newstring, ss->string, ss->length);
	/* zeroize string before freeing; as it may be sensitive */
	memset(ss->string, 0, ss->allocatedLength);
	free(ss->string);
	ss->string = newstring;
	ss->allocatedLength = newsize;
    }
    return ss;
}

/* appendCharToSimpleString(c,ss)
 * Appends the character c to the end of simple string ss.
 * Reallocates storage assigned to s if necessary to make room for c.
 */
void appendCharToSimpleString(int c, sexpSimpleString ss)
{
    if (ss == NULL) {
	ss = newSimpleString();
	if (ss == NULL) return;
    }
/*@-branchstate@*/
    if (ss->string == NULL || ss->length == ss->allocatedLength )
	ss = reallocateSimpleString(ss);
/*@=branchstate@*/
    if (ss != NULL && ss->string != NULL)
	ss->string[ss->length++] = (octet) (c & 0xFF);
}

/****************************/
/* SEXP STRING MANIPULATION */
/****************************/

/* newSexpString()
 * Creates and initializes a new sexpString object.
 * Both the presentation hint and the string are initialized to NULL.
 */
sexpString newSexpString(void)
{
    sexpString s = sexpAlloc(sizeof(*s));

    s->type = SEXP_STRING;
    s->presentationHint = NULL;
    s->string = NULL;
    return s;
}

/* sexpStringPresentationHint()
 * returns presentation hint field of the string
 */
sexpSimpleString sexpStringPresentationHint(sexpString s)
{
    return s->presentationHint;
}

/* setSexpStringPresentationHint()
 * assigns the presentation hint field of the string
 */
void setSexpStringPresentationHint(sexpString s, sexpSimpleString ss)
{
/*@-assignexpose@*/
    s->presentationHint = ss;
/*@=assignexpose@*/
}

/* setSexpStringString()
 * assigns the string field of the string
 */
void setSexpStringString(sexpString s, sexpSimpleString ss)
{
/*@-assignexpose@*/
    s->string = ss;
/*@=assignexpose@*/
}

/* sexpStringString()
 * returns the string field of the string
 */
sexpSimpleString sexpStringString(sexpString s)
{
    return s->string;
}

/* closeSexpString()
 * finish up string computations after created
 */
void closeSexpString(/*@unused@*/ sexpString s)
{ ; }  /* do nothing in this implementation */

/**************************/
/* SEXP LIST MANIPULATION */
/**************************/

/* newSexpList()
 * Creates and initializes a new sexpList object.
 * Both the first and rest fields are initialized to NULL, which is
 * SEXP's representation of an empty list.
 */
sexpList newSexpList(void)
{
    sexpList list = sexpAlloc(sizeof(*list));

    list->type = SEXP_LIST;
    list->first = NULL;
    list->rest = NULL;
    return list;
}

/* sexpAddSexpListObject()
 * add object to end of list
 */
void sexpAddSexpListObject(sexpList list, sexpObject object)
{
    if (list->first == NULL) {
/*@-assignexpose@*/
	list->first = object;
/*@=assignexpose@*/
    } else {
	while (list->rest != NULL)
	    list = list->rest;
	list->rest = newSexpList();
/*@-branchstate@*/
	if ((list = list->rest) != NULL) {
/*@-assignexpose@*/
	    list->first = object;
/*@=assignexpose@*/
	}
/*@=branchstate@*/
    }
}

/* closeSexpList()
 * finish off a list that has just been input
 */
void closeSexpList(/*@unused@*/ sexpList list)
{ ; } /* nothing in this implementation */

/* Iteration on lists.
   To accomodate different list representations, we introduce the
   notion of an "iterator".
*/

/* sexpListIter()
 * return the iterator for going over a list
 */
sexpIter sexpListIter(sexpList list)
{
/*@-castexpose@*/
    return (sexpIter)list;
/*@=castexpose@*/
}

/* sexpIterNext()
 * advance iterator to next element of list, or else return null
 */
sexpIter sexpIterNext(sexpIter iter)
{
    if (iter == NULL) return NULL;
    return (sexpIter)(((sexpList)iter)->rest);
}

/* sexpIterObject ()
 * return object corresponding to current state of iterator
 */
sexpObject sexpIterObject(sexpIter iter)
{
    if (iter == NULL) return NULL;
    return ((sexpList)iter)->first;
}

/****************************/
/* SEXP OBJECT MANIPULATION */
/****************************/

int isObjectString(sexpObject object)
{
    if (((sexpString)object)->type == SEXP_STRING) return TRUE;
    else                                           return FALSE;
}

int isObjectList(sexpObject object)
{
    if (((sexpList)object)->type == SEXP_LIST) return TRUE;
    else                                       return FALSE;
}
