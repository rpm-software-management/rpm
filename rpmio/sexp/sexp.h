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
  octet *string;
} sexpSimpleString;

/* sexpString */
typedef struct sexp_string {
  int type;
  sexpSimpleString *presentationHint;
  sexpSimpleString *string;
} sexpString;

/* sexpList */
/* If first is NULL, then rest must also be NULL; this is empty list */
typedef struct sexp_list {  
  char type;
  union sexp_object *first;
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
	/*@*/;
void initializeMemory(void)
	/*@*/;
char *sexpAlloc(int n)
	/*@*/;
sexpSimpleString *newSimpleString(void)
	/*@*/;
long int simpleStringLength(sexpSimpleString *ss)
	/*@*/;
octet *simpleStringString(sexpSimpleString *ss)
	/*@*/;
sexpSimpleString *reallocateSimpleString(sexpSimpleString *ss)
	/*@*/;
void appendCharToSimpleString(int c, sexpSimpleString *ss)
	/*@*/;
sexpString *newSexpString(void)
	/*@*/;
sexpSimpleString *sexpStringPresentationHint(sexpString *s)
	/*@*/;
void setSexpStringPresentationHint(sexpString *s, sexpSimpleString *ss)
	/*@*/;
void setSexpStringString(sexpString *s, sexpSimpleString *ss)
	/*@*/;
sexpSimpleString *sexpStringString(sexpString *s)
	/*@*/;
void closeSexpString(sexpString *s)
	/*@*/;
sexpList *newSexpList(void)
	/*@*/;
void sexpAddSexpListObject(sexpList *list, sexpObject *object)
	/*@*/;
void closeSexpList(sexpList *list)
	/*@*/;
sexpIter *sexpListIter(sexpList *list)
	/*@*/;
sexpIter *sexpIterNext(sexpIter *iter)
	/*@*/;
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
	/*@*/;
void getChar(sexpInputStream *is)
	/*@*/;
sexpInputStream *newSexpInputStream(void)
	/*@*/;
void skipWhiteSpace(sexpInputStream *is)
	/*@*/;
void skipChar(sexpInputStream *is, int c)
	/*@*/;
void scanToken(sexpInputStream *is, sexpSimpleString *ss)
	/*@*/;
sexpObject *scanToEOF(sexpInputStream *is)
	/*@*/;
unsigned long int scanDecimal(sexpInputStream *is)
	/*@*/;
void scanVerbatimString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@*/;
void scanQuotedString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@*/;
void scanHexString(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@*/;
void scanBase64String(sexpInputStream *is, sexpSimpleString *ss, long int length)
	/*@*/;
sexpSimpleString *scanSimpleString(sexpInputStream *is)
	/*@*/;
sexpString *scanString(sexpInputStream *is)
	/*@*/;
sexpList *scanList(sexpInputStream *is)
	/*@*/;
sexpObject *scanObject(sexpInputStream *is)
	/*@*/;

/* sexp-output */
void putChar(sexpOutputStream *os, int c)
	/*@*/;
void varPutChar(sexpOutputStream *os, int c)
	/*@*/;
void changeOutputByteSize(sexpOutputStream *os, int newByteSize, int mode)
	/*@*/;
void flushOutput(sexpOutputStream * os)
	/*@*/;
void newLine(sexpOutputStream *os, int mode)
	/*@*/;
sexpOutputStream *newSexpOutputStream(void)
	/*@*/;
void printDecimal(sexpOutputStream *os, long int n)
	/*@*/;
void canonicalPrintVerbatimSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
void canonicalPrintString(sexpOutputStream *os, sexpString *s)
	/*@*/;
void canonicalPrintList(sexpOutputStream *os, sexpList *list)
	/*@*/;
void canonicalPrintObject(sexpOutputStream *os, sexpObject *object)
	/*@*/;
void base64PrintWholeObject(sexpOutputStream *os, sexpObject *object)
	/*@*/;
int canPrintAsToken(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
void advancedPrintTokenSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleStringToken(sexpSimpleString *ss)
	/*@*/;
void advancedPrintVerbatimSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleStringVerbatim(sexpSimpleString *ss)
	/*@*/;
void advancedPrintBase64SimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
void advancedPrintHexSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleStringHexadecimal(sexpSimpleString *ss)
	/*@*/;
int canPrintAsQuotedString(sexpSimpleString *ss)
	/*@*/;
void advancedPrintQuotedStringSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleStringQuotedString(sexpSimpleString *ss)
	/*@*/;
void advancedPrintSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
void advancedPrintString(sexpOutputStream *os, sexpString *s)
	/*@*/;
int advancedLengthSimpleStringBase64(sexpSimpleString *ss)
	/*@*/;
int advancedLengthSimpleString(sexpOutputStream *os, sexpSimpleString *ss)
	/*@*/;
int advancedLengthString(sexpOutputStream *os, sexpString *s)
	/*@*/;
int advancedLengthList(sexpOutputStream *os, sexpList *list)
	/*@*/;
void advancedPrintList(sexpOutputStream *os, sexpList *list)
	/*@*/;
void advancedPrintObject(sexpOutputStream *os, sexpObject *object)
	/*@*/;
