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

typedef unsigned char octet;

/* TYPES OF OBJECTS */
#define SEXP_STRING 1
#define SEXP_LIST   2

/* sexpSimpleString */
typedef struct sexp_simplestring { 
  long int length;
  long int allocatedLength;
/*@null@*/
  octet *string;
} sexpSimpleString;

/* sexpString */
typedef struct sexp_string {
  int type;
/*@null@*/
  sexpSimpleString *presentationHint;
/*@null@*/
  sexpSimpleString *string;
} sexpString;

/* sexpList */
/* If first is NULL, then rest must also be NULL; this is empty list */
typedef struct sexp_list {  
  char type;
/*@null@*/
  union sexp_object *first;
/*@null@*/
  struct sexp_list  *rest;
} sexpList;

/* sexpObject */
/* so we can have a pointer to something of either type */
typedef union sexp_object {
  sexpString string;
  sexpList list;
} sexpObject;

/* sexpIter */
/* an "iterator" for going over lists */
/* In this implementation, it is the same as a list */
typedef sexpList sexpIter;

typedef struct sexp_inputstream {
  int nextChar;        /* character currently being scanned */
  int byteSize;        /* 4 or 6 or 8 == currently scanning mode */
  int bits;            /* Bits waiting to be used */
  int nBits;           /* number of such bits waiting to be used */
  void (*getChar)();
  int count;           /* number of 8-bit characters output by getChar */
  FILE *inputFile;     /* where to get input, if not stdin */
} sexpInputStream;

typedef struct sexp_outputstream {
  long int column;          /* column where next character will go */
  long int maxcolumn;       /* max usable column, or -1 if no maximum */
  long int indent;          /* current indentation level (starts at 0) */
  void (*putChar)();        /* output a character */
  void (*newLine)();        /* go to next line (and indent) */
  int byteSize;             /* 4 or 6 or 8 depending on output mode */
  int bits;                 /* bits waiting to go out */
  int nBits;                /* number of bits waiting to go out */
  long int base64Count;     /* number of hex or base64 chars printed 
			       this region */
  int mode;                 /* BASE64, ADVANCED, or CANONICAL */
  FILE *outputFile;         /* where to put output, if not stdout */
} sexpOutputStream;

/* Function prototypes */

/* sexp-basic */
void ErrorMessage(int level, const char *msg, int c1, int c2)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void initializeMemory(void)
	/*@*/;
char *sexpAlloc(int n)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
sexpSimpleString *newSimpleString(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
long int simpleStringLength(sexpSimpleString *ss)
	/*@*/;
/*@null@*/
octet *simpleStringString(sexpSimpleString *ss)
	/*@*/;
/*@null@*/
sexpSimpleString *reallocateSimpleString(sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies ss, fileSystem @*/;
void appendCharToSimpleString(int c, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies ss, fileSystem @*/;
sexpString *newSexpString(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@null@*/
sexpSimpleString *sexpStringPresentationHint(sexpString *s)
	/*@*/;
void setSexpStringPresentationHint(sexpString *s, sexpSimpleString *ss)
	/*@modifies s @*/;
void setSexpStringString(sexpString *s, sexpSimpleString *ss)
	/*@modifies s @*/;
/*@null@*/
sexpSimpleString *sexpStringString(sexpString *s)
	/*@*/;
void closeSexpString(sexpString *s)
	/*@*/;
sexpList *newSexpList(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void sexpAddSexpListObject(sexpList *list, sexpObject *object)
	/*@globals fileSystem @*/
	/*@modifies list, fileSystem @*/;
void closeSexpList(sexpList *list)
	/*@*/;
sexpIter *sexpListIter(sexpList *list)
	/*@*/;
/*@null@*/
sexpIter *sexpIterNext(sexpIter *iter)
	/*@*/;
/*@null@*/
sexpObject *sexpIterObject(sexpIter *iter)
	/*@*/;
int isObjectString(sexpObject *object)
	/*@*/;
int isObjectList(sexpObject *object)
	/*@*/;

/* sexp-input */
void initializeCharacterTables(void)
	/*@*/;
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
int isAlpha(int c)
	/*@*/;
void changeInputByteSize(sexpInputStream *is, int newByteSize)
	/*@modifies is @*/;
void getChar(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpInputStream *newSexpInputStream(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void skipWhiteSpace(sexpInputStream *is)
	/*@modifies is @*/;
void skipChar(sexpInputStream *is, int c)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
void scanToken(sexpInputStream *is, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
sexpObject *scanToEOF(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
unsigned long int scanDecimal(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
void scanVerbatimString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanQuotedString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanHexString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
void scanBase64String(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@globals fileSystem @*/
	/*@modifies is, ss, fileSystem @*/;
sexpSimpleString *scanSimpleString(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpString *scanString(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpList *scanList(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;
sexpObject *scanObject(sexpInputStream *is)
	/*@globals fileSystem @*/
	/*@modifies is, fileSystem @*/;

/* sexp-output */
void putChar(sexpOutputStream *os, int c)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void varPutChar(sexpOutputStream *os, int c)
	/*@modifies os @*/;
void changeOutputByteSize(sexpOutputStream *os, int newByteSize, int mode)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void flushOutput(sexpOutputStream * os)
	/*@modifies os @*/;
void newLine(sexpOutputStream *os, int mode)
	/*@modifies os @*/;
sexpOutputStream *newSexpOutputStream(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void printDecimal(sexpOutputStream *os, long int n)
	/*@modifies os @*/;
void canonicalPrintVerbatimSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintString(sexpOutputStream *os, sexpString *s)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintList(sexpOutputStream *os, sexpList *list)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void canonicalPrintObject(sexpOutputStream *os, sexpObject *object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void base64PrintWholeObject(sexpOutputStream *os, sexpObject *object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int canPrintAsToken(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
void advancedPrintTokenSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@modifies os @*/;
int advancedLengthSimpleStringToken(sexpSimpleString *ss)
	/*@*/;
void advancedPrintVerbatimSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int advancedLengthSimpleStringVerbatim(sexpSimpleString *ss)
	/*@*/;
void advancedPrintBase64SimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintHexSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int advancedLengthSimpleStringHexadecimal(sexpSimpleString *ss)
	/*@*/;
int canPrintAsQuotedString(sexpSimpleString *ss)
	/*@*/;
void advancedPrintQuotedStringSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@modifies os @*/;
int advancedLengthSimpleStringQuotedString(sexpSimpleString *ss)
	/*@*/;
void advancedPrintSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintString(sexpOutputStream *os, sexpString *s)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
int advancedLengthSimpleStringBase64(sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthString(sexpOutputStream *os, sexpString *s)
	/*@*/;
int advancedLengthList(sexpOutputStream *os, sexpList *list)
	/*@*/;
void advancedPrintList(sexpOutputStream *os, sexpList *list)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
void advancedPrintObject(sexpOutputStream *os, sexpObject *object)
	/*@globals fileSystem @*/
	/*@modifies os, fileSystem @*/;
