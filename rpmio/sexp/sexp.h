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
void ErrorMessage();
void initializeMemory();
char *sexpAlloc();
sexpSimpleString *newSimpleString();
long int simpleStringLength();
octet *simpleStringString();
sexpSimpleString *reallocateSimpleString();
void appendCharToSimpleString();
sexpString *newSexpString();
sexpSimpleString *sexpStringPresentationHint();
sexpSimpleString *sexpStringString();
void setSexpStringPresentationHint();
void setSexpStringString();
void closeSexpString();
sexpList *newSexpList();
void sexpAddSexpListObject();
void closeSexpList();
sexpIter *sexpListIter();
sexpIter *sexpIterNext();
sexpObject *sexpIterObject();
int isObjectString();
int isObjectList();

/* sexp-input */
void initializeCharacterTables();
int isWhiteSpace();
int isDecDigit();
int isHexDigit();
int isBase64Digit();
int isTokenChar();
int isAlpha();
void changeInputByteSize();
void getChar();
sexpInputStream *newSexpInputStream();
void skipWhiteSpace();
void skipChar();
void scanToken();
sexpObject *scanToEOF();
unsigned long int scanDecimal();
void scanVerbatimString();
void scanQuotedString();
void scanHexString();
void scanBase64String();
sexpSimpleString *scanSimpleString();
sexpString *scanString();
sexpList *scanList();
sexpObject *scanObject();

/* sexp-output */
void putChar();
void varPutChar();
void changeOutputByteSize();
void flushOutput();
void newLine();
sexpOutputStream *newSexpOutputStream();
void printDecimal();
void canonicalPrintVerbatimSimpleString();
void canonicalPrintString();
void canonicalPrintList();
void canonicalPrintObject();
void base64PrintWholeObject();
int canPrintAsToken();
int significantNibbles();
void advancedPrintTokenSimpleString();
int  advancedLengthSimpleStringToken();
void advancedPrintVerbatimSimpleString();
int advancedLengthSimpleStringVerbatim();
void advancedPrintBase64SimpleString();
void advancedPrintHexSimpleString();
int canPrintAsQuotedString();
void advancedPrintQuotedStringSimpleString();
void advancedPrintSimpleString();
void advancedPrintString();
int  advancedLengthSimpleStringBase64();
int  advancedLengthSimpleString();
int  advancedLengthString();
int  advancedLengthList();
void advancedPrintList();
void advancedPrintObject();
