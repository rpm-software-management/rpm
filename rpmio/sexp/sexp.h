/* SEXP standard header file: sexp.h 
 * Ronald L. Rivest
 * 6/29/1997
 */

/* GLOBAL DECLARATIONS */
#define TRUE    1
#define FALSE   0

/* PRINTING MODES */
#define CANONICAL 1    /* standard for hashing and tranmission */
#define BASE64    2    /* base64 version of canonical */
#define ADVANCED  3    /* pretty-printed */

/* ERROR MESSAGE LEVELS */
#define WARNING 1
#define ERROR 2

#define DEFAULTLINELENGTH 75

/* TYPES OF OBJECTS */
#define SEXP_STRING 1
#define SEXP_LIST   2

typedef unsigned char octet;

typedef /*@abstract@*/ struct sexpSimpleString_s * sexpSimpleString;
typedef /*@abstract@*/ struct sexpString_s * sexpString;
typedef /*@abstract@*/ struct sexpList_s * sexpList;
typedef /*@abstract@*/ union sexpObject_u * sexpObject;

typedef /*@abstract@*/ struct sexpInputStream_s * sexpInputStream;
typedef /*@abstract@*/ struct sexpOutputStream_s * sexpOutputStream;

/* sexpSimpleString */
struct sexpSimpleString_s { 
  long int length;
  long int allocatedLength;
/*@null@*/
  octet *string;
};

/* sexpString */
struct sexpString_s {
  int type;
/*@null@*/
  sexpSimpleString presentationHint;
/*@null@*/
  sexpSimpleString string;
};

/* sexpObject */

/* sexpList */
/* If first is NULL, then rest must also be NULL; this is empty list */
struct sexpList_s {  
    int type;
/*@null@*/
    sexpObject first;
/*@null@*/
    sexpList rest;
};

/* sexpObject */
/* so we can have a pointer to something of either type */
union sexpObject_u {
/*@unused@*/
  struct sexpString_s string;
/*@unused@*/
  struct sexpList_s list;
};

/* sexpIter */
/* an "iterator" for going over lists */
/* In this implementation, it is the same as a list */
typedef /*@abstract@*/ sexpList * sexpIter;

/* Function prototypes */

/* sexp-basic */
/*@mayexit@*/ /*@printflike@*/
void ErrorMessage(int level, const char *fmt, ...)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void initializeMemory(void)
	/*@*/;
char *sexpAlloc(int n)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
sexpSimpleString newSimpleString(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
long int simpleStringLength(sexpSimpleString ss)
	/*@*/;
/*@exposed@*/ /*@null@*/
octet *simpleStringString(sexpSimpleString ss)
	/*@*/;
/*@null@*/
sexpSimpleString reallocateSimpleString(sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies ss, fileSystem @*/;
void appendCharToSimpleString(int c, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies ss, fileSystem @*/;
sexpString newSexpString(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@exposed@*/ /*@null@*/
sexpSimpleString sexpStringPresentationHint(sexpString s)
	/*@*/;
void setSexpStringPresentationHint(sexpString s, sexpSimpleString ss)
	/*@modifies s @*/;
void setSexpStringString(sexpString s, sexpSimpleString ss)
	/*@modifies s @*/;
/*@exposed@*/ /*@null@*/
sexpSimpleString sexpStringString(sexpString s)
	/*@*/;
void closeSexpString(sexpString s)
	/*@*/;
sexpList newSexpList(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void sexpAddSexpListObject(sexpList list, sexpObject object)
	/*@globals fileSystem @*/
	/*@modifies list, fileSystem @*/;
void closeSexpList(sexpList list)
	/*@*/;
/*@observer@*/
sexpIter sexpListIter(sexpList list)
	/*@*/;
/*@exposed@*/ /*@observer@*/ /*@null@*/
sexpIter sexpIterNext(sexpIter iter)
	/*@*/;
/*@exposed@*/ /*@observer@*/ /*@null@*/
sexpObject sexpIterObject(sexpIter iter)
	/*@*/;
int isObjectString(sexpObject object)
	/*@*/;
int isObjectList(sexpObject object)
	/*@*/;

/* sexp-input */
void initializeCharacterTables(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/;
int isWhiteSpace(int c)
	/*@*/;
int isDecDigit(int c)
	/*@*/;
int isHexDigit(int c)
	/*@*/;
int isBase64Digit(int c)
	/*@*/;
int isTokenChar(int c)
	/*@*/;
/*@unused@*/
int isAlpha(int c)
	/*@*/;
void changeInputByteSize(sexpInputStream is, int newByteSize)
	/*@modifies is @*/;
void getChar(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;

/*@-globuse@*/
void sexpIFgetc(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
/*@=globuse@*/
int sexpIFpeek(sexpInputStream is)
	/*@*/;
int sexpIFeof(sexpInputStream is)
	/*@*/;
void sexpIFpoke(sexpInputStream is, int c)
	/*@modifies is @*/;
sexpInputStream sexpIFopen(/*@null@*/ const char * ifn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void skipWhiteSpace(sexpInputStream is)
	/*@modifies is @*/;
void skipChar(sexpInputStream is, int c)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
void scanToken(sexpInputStream is, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
sexpObject scanToEOF(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
unsigned long int scanDecimal(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
void scanVerbatimString(sexpInputStream is, sexpSimpleString ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanQuotedString(sexpInputStream is, sexpSimpleString ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanHexString(sexpInputStream is, sexpSimpleString ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanBase64String(sexpInputStream is, sexpSimpleString ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
sexpSimpleString scanSimpleString(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpString scanString(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpList scanList(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpObject scanObject(sexpInputStream is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;

/* sexp-output */
void putChar(sexpOutputStream os, int c)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void varPutChar(sexpOutputStream os, int c)
	/*@modifies os @*/;
void changeOutputByteSize(sexpOutputStream os, int newByteSize, int mode)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void flushOutput(sexpOutputStream os)
	/*@modifies os @*/;
void newLine(sexpOutputStream os, int mode)
	/*@modifies os @*/;

void sexpOFnewLine(sexpOutputStream os, int mode)
	/*@modifies os @*/;
void sexpOFwidth(sexpOutputStream os, int width)
	/*@modifies os @*/;
sexpOutputStream sexpOFopen(/*@null@*/ const char * ofn, const char * fmode)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

void printDecimal(sexpOutputStream os, long int n)
	/*@modifies os @*/;
void canonicalPrintVerbatimSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintString(sexpOutputStream os, sexpString s)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintList(sexpOutputStream os, sexpList list)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintObject(sexpOutputStream os, sexpObject object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void base64PrintWholeObject(sexpOutputStream os, sexpObject object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int canPrintAsToken(sexpOutputStream os, sexpSimpleString ss)
	/*@*/;
void advancedPrintTokenSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@modifies os @*/;
int advancedLengthSimpleStringToken(sexpSimpleString ss)
	/*@*/;
/*@unused@*/
void advancedPrintVerbatimSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
/*@unused@*/
int advancedLengthSimpleStringVerbatim(sexpSimpleString ss)
	/*@*/;
void advancedPrintBase64SimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintHexSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int advancedLengthSimpleStringHexadecimal(sexpSimpleString ss)
	/*@*/;
int canPrintAsQuotedString(sexpSimpleString ss)
	/*@*/;
void advancedPrintQuotedStringSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@modifies os @*/;
int advancedLengthSimpleStringQuotedString(sexpSimpleString ss)
	/*@*/;
void advancedPrintSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintString(sexpOutputStream os, sexpString s)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int advancedLengthSimpleStringBase64(sexpSimpleString ss)
	/*@*/;
int advancedLengthSimpleString(sexpOutputStream os, sexpSimpleString ss)
	/*@*/;
int advancedLengthString(sexpOutputStream os, sexpString s)
	/*@*/;
int advancedLengthList(sexpOutputStream os, sexpList list)
	/*@*/;
void advancedPrintList(sexpOutputStream os, sexpList list)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintObject(sexpOutputStream os, sexpObject object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
