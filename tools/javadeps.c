/*
RPM and it's source code are covered under two separate licenses. 

The entire code base may be distributed under the terms of the GNU General
Public License (GPL), which appears immediately below.  Alternatively,
all of the source code in the lib subdirectory of the RPM source code
distribution as well as any code derived from that code may instead be
distributed under the GNU Library General Public License (LGPL), at the
choice of the distributor. The complete text of the LGPL appears
at the bottom of this file.

This alternatively is allowed to enable applications to be linked against
the RPM library (commonly called librpm) without forcing such applications
to be distributed under the GPL. 

Any questions regarding the licensing of RPM should be addressed to
marc@redhat.com and ewt@redhat.com.
*/

/* 
   Simple progam for pullng all the referenced java classes out of a
   class file.  Java files are supposed to be platform independent, so
   this program should work on all platforms.  This code is based on 
   the information found in:

       "Java Virtual Machine" by Jon Meyer & Troy Downing.
          O'Reilly & Associates, INC. (First Edition, March 1997)
          ISBN: 1-56592-194-1

   Jonathan Ross, Ken Estes
   Mail.com
 */


/* 
   Remember that: 

   JAR consists of a zip archive, as defined by PKWARE, containing
   a manifest file and potentially signature files, as defined in
   the Manifest and Signature specification.  So we use infozip's 
   'unzip -p' found at http://www.cdrom.com/pub/infozip/.

   Additional documentation, about this fact, at:

   http://java.sun.com/products/jdk/1.1/docs/guide/jar/jarGuide.html
   http://java.sun.com/products/jdk/1.2/docs/guide/jar/jarGuide.html
   
   http://java.sun.com/products/jdk/1.1/docs/guide/jar/manifest.html
   http://java.sun.com/products/jdk/1.2/docs/guide/jar/manifest.html
   
*/

#include "system.h"

/*
  these includes are for my use, rpm will use #include "system.h"*
*/

/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
*/

#include <stdarg.h>

/*---------typedefs---------*/


/* The symbol table is a waste of memory.. 
   but it's easy to code! */

typedef struct {
  short poolSize;
  char **stringList;
  short *classRef;
  short *typeRef;
} symbolTable_t;


/*---------Global Variables, in all caps---------*/

/*name of this program*/
char *PROGRAM_NAME=0;

/*name of the current class file*/
char *FILE_NAME=0;

/*the name of the last class file seen*/
char *CLASS_NAME=0;

/*arguments chosen*/
int ARG_PROVIDES=0;
int ARG_REQUIRES=0;
int ARG_RPMFORMAT=0;
int ARG_KEYWORDS=0;
int ARG_STARPROV=0;

/*keywords found in class file*/
char *KEYWORD_VERSION=0;
char *KEYWORD_REVISION=0;
char *KEYWORD_EPOCH=0;

/* 

   Quantify says over 80 percent of the time of the program is spent
   in printf (and the functions it calls) AND I verified that only
   about a quarter of the classes found in the dependency analysis are
   unique. After the change no function used more then 26% of the over
   all time.

   I implement a simple mechanism to remove most duplicate dependencies.
   The print_table is a table of string which are to be printed.  Just
   before the program exists it is sorted and all unique entries are
   printed.  If it fills up during then it is flushed using the same
   technique as above. 

   The functions which manipulate the table are:
	void print_table_flush(void)
	void print_table_add(char *str)

*/


#define MAX_PRINT_TABLE 10000
char *PRINT_TABLE[MAX_PRINT_TABLE];
int SIZE_PRINT_TABLE;

/*--------- declare all functions ---------*/

void usage (void);
void outofmemory(void);
void die(char *format, ...);
size_t my_fread(void *ptr, size_t size, size_t nitems, FILE *stream);
void check_range(short value, short poolSize);
char *is_lower_equal (char *string, char *pattern);
int findJavaMagic (FILE *fileHandle);
int my_strcmp (const void *a, const void *b);
void print_table_flush(void);
void print_table_add(char *str);
char *formatClassName(char *pSomeString, char terminator, char print_star);
void dumpRefType(char *pSomeString);
void dumpRequires(symbolTable_t *symbolTable);
void genSymbolTable (FILE *fileHandle, symbolTable_t *symbolTable);
void findClassName  (FILE *fileHandle, symbolTable_t *symbolTable);
void freeSymbolTable (symbolTable_t *symbolTable);
void processJavaFile (FILE *fileHandle);

/*--------- functions ---------*/

void
usage (void)
{
  printf("NAME:\n\tjavadeps - Examine Java class files and\n"
	 "\t\t\treturn information about their dependencies.\n\n");
  printf("USAGE:\n");
  printf("\t javadeps { --provides | --requires } \n"
	 "\t\t   [--rpmformat] [--keywords] \n"
	 "\t\t     [--] classfile-name ... \n\n"
	 "\t javadeps [--help]\n\n");
  printf("\n\n");
  printf("DESCRIPTION:\n\n");
  printf("List the dependencies or the fully qualified class names, of the \n"
	 "classfiles listed on the command line. \n\n");
  printf("OPTIONS:\n\n");
  printf("--requires  For each class files listed in the arguments,\n"
	 " -r         print the list of class files that would be\n"
	 "            required to run these java programs.  This does not \n"
	 "            include anyting instantiated by reflection.\n\n");
  printf("--provides  For each class files listed in the arguments, \n"
	 " -p         Print the fully qualified java classes,\n"
	 "            that they provide.\n\n");
  printf("--rpmformat format the output to match that used by RPM's \n"
	 " -F         (Red Hat Package Manager) dependency analysis  \n"
	 "            database. The default is not --rpmformat.\n\n");
  printf("--keywords  Make use of any keywords embeded in the classfile.\n"
	 " -k         The default is not --keyword.\n\n");
  printf("--starprov  Add the star notation provides to the provides list.\n"
	 " -s         The default is not --starprov.  This is only for use\n"
	 "            with (Sun) jhtml dependencies, and since jhtml is \n"
	 "            deprecated so is this option.\n\n");
  printf("--help      Display this page and exit.\n\n");
  printf("--          This stops the processing of arguments, making it \n"
	 "            easier for users to have filenames like '--keywords',\n"
	 "            without the command line parser getting confused.\n\n");
  printf("\n\n");
  printf("If any of the class file names in the argument list is '-' then\n"
	 "<stdin> will be read instead of reading from a file.  The\n"
	 "contents of <stdin> should be the contents of a class file and \n"
	 "not a list of class files to read.  It is assumed that when run \n"
	 "with '-', this program is in a pipeline preceeded by the \n"
	 "command 'unzip -p filename.jar' so that <stdin> may contain\n"
	 "the contents of several classfiles concatenated together.\n");
  printf("\n\n");
  printf("If --keywords is specified then the following strings are \n"
	 "searched for (case insensitive) in the class file string table\n"
	 "and, if a string is found with a prefix matching the keyword then \n"
	 "the dependencies are changed accordingly.  There may be multiple \n"
	 "string tables entries prefixed with RPM_Provides and RPM_Requires. \n"
	 "This would indicate that the dependency is the union\n"
	 "of all entries.\n"
	 "\n\n"
	 "Keyword List:\n\n"
	 "'$Revision: '     This RCS/CVS compatible keyword is assumed to \n"
	 "                  contain the version number of the class file \n"
	 "                  it is found in.  Care should be taken with this\n" 
	 "                  option as RPM's notion of which version is later\n"
	 "                  may not corrispond with your own, especially\n"
	 "                  if you use branches. This keyword\n"
	 "                  only effects the output of --provides and only\n"
	 "                  when RPM_Version is not defined.\n\n"
	 "'RPM_Version: '   This is an alternative method of specifing the\n"
	 "                  version number of the class.  It will override\n"
	 "                  $Revision if set.   This keyword only effects\n"
	 "                  the output of --provides \n\n"
	 "'RPM_Epoch: '     This string contains the epoch to use with the \n"
	 "                  version number stored in Revision.  If not \n"
	 "                  specified, the epoch is assumed to be zero.\n"
	 "                  This keyword only effects the output of\n "
	 "                  --provides and only when $Revision number is\n"
	 "                  used.\n\n"
	 "'RPM_Provides: '  This string lists additional capabilites\n"
	 "                  provided by the java class.  The string should\n"
	 "                  be  a white space ([\\t\\v\\n\\r\\f\\ ])\n"
	 "                  separated list of dependency strings.  Each\n"
	 "                  dependency string must be of the same format as\n"
	 "                  would be valid in the Requires or Provides line\n"
	 "                  of the specfile. This keyword only effects the\n"
	 "                  output of --provides.\n\n"
	 "'RPM_Requires: '  This string lists additional requirements of\n"
	 "                  the java class.  The string should be a white \n"
	 "                  space ([\\t   \v\\n\\r\\f\\ ]) separated list of \n"
	 "                  dependency strings.  Each dependency string must\n"
	 "                  be of the same format as would be valid in the \n"
	 "                  Requires or Provides line of the specfile.  This\n"
	 "                  keyword only effects the output of --requires.\n "
	 "                  \n\n"
	 "Note that there is no means of setting the release number.  This\n"
	 "is necessary because release numbers are incremented when the\n"
	 "source does not change but the package needs to be rebuilt.  So\n"
	 "relase numbers can not be stored in the source.  The release is\n"
	 "assumed to be zero. \n\n"
	 "");
  printf("EXAMPLES (Java Keywords): \n\n"
	 "\t public static final String REVISION = \"$Revision: 2.7 $\";\n"
	 "\t public static final String EPOCH = \"4\";\n"
	 "\t public static final String REQUIRES = \"RPM_Requires: "
	 "java(gnu.regexp.RE) java(com.ibm.site.util.Options)>=1.5\";\n"
	 "");
  printf("\n\n");
  printf("EXAMPLES (Arguments): \n\n"
	 "\tjavadeps --requires -- filename.class\n\n"
	 "\tjavadeps --provides -- filename.class\n\n"
	 "\tjavadeps --help\n\n"
	 "\n"
	 "\tjavadeps --requires --rpmformat --keywords -- filename.class\n\n"
	 "\tjavadeps --requires -- filename1.class filename2.class\n\n"
	 "\tcat filename2.class | javadeps --requires -- filename1.class -\n\n"
	 "\tunzip -p filename.jar | javadeps --requires -- - \n\n"
	 "");
  printf("This program is distributed with RPM the Redhat Package \n"
	 "Managment system.  Further information about RPM can be found at \n"
	 "\thttp://www.rpm.org/\n\n");
  printf("\n\n");
  exit(-1);
}


void outofmemory(void) {

  /* Its doubtful we could do a printf if there is really a memory
    issue but at least halt the program */

  fprintf(stderr, "Could not allocate memory");
  exit(-1);
}


void die(char *format, ...) {
  /* Most errors are fatal.
     This function throws a fatal error and 
     accepts arguments like printf does*/

  char  *newformat = NULL, *newmsg = NULL;
  va_list ap;

  if ( !(newformat = malloc(1024)) || !(newmsg = malloc(1024)) )
    outofmemory();

  /* Rewrite format line, to include additional information.  The
     format line we chose depends on how much information is availible
     at the time of the error.  Display the maximum ammount of
     information. */

  /* notice the FILE_NAME is useless for jar files.  We would want to
     print the name of the classfile which caused the error.  That
     is hard since we only know that name when we are done parsing
     the file, and most errors will occur before that.*/

  va_start(ap, format);
  
  if ( (!FILE_NAME) ) {

    sprintf (newformat, "\n%s: %s",
	     PROGRAM_NAME, format);

  } else if ( (FILE_NAME) && (!CLASS_NAME) ) {
    
    sprintf (newformat, "\n%s: Java classfile: %s, %s",
	     PROGRAM_NAME, FILE_NAME, format);
    
  } else if (CLASS_NAME) {
    sprintf (newformat, "\n%s: Java classfile: %s, classname: %s, %s",
	     PROGRAM_NAME, FILE_NAME, CLASS_NAME, format);
  }
    
  vsprintf (newmsg, newformat, ap);  
  
  /* print error to where it needs to go:
	 stdout, stderr, or syslog
  */

  fprintf(stderr, newmsg);
  
  free(newformat);
  free(newmsg);
  
  exit(-1);
}


/* wrap fread for safety.  It is a fatal error to get an unexpected
   EOF inside a class file. */

size_t my_fread(void *ptr, size_t size, size_t nitems, FILE *stream) {
  size_t rc=0;
  /*these variables are helpful in the debugger*/
  int eof=0;
  int error=0;


  rc = fread(ptr, size, nitems, stream);
  if ( (size!=0) && (rc == 0) ) {
    eof = feof(stream);
    error = ferror(stream);
    die("Error reading from file, or unexpected EOF\n");
  }
  return rc;
}


void check_range(short value, short poolSize) {

  if (value > poolSize) {
    die("Value: %d, is out of range of the constant pool\n",
	value);
  }
  return ;
}
 


/* If lower case conversion of string is equal to pattern return a
   pointer into string, just after the match.  If the string does not
   patch the pattern the null pointer is returned.  This does not
   change string. 

   This is similar to strcasecmp, but I expect my patterns to be a
   prefix of my strings. */

char 
*is_lower_equal (char *string, char *pattern) 
{
  
  while ( (tolower(*string) == *pattern) && 
	  *string && *pattern )  {
    string++; 
    pattern++;
  }

  if ( *pattern == 0 ) {
    return string;
  } 

  return NULL;
}


/*  
   Read fileHandle until we find the next instance of the Java
   Classfile magic number indicating a java file or find EOF or
   fileread error. Since we are reading from stdin which may contain
   the concatination of many class files we can not be sure that the
   magic number will be the first few bytes.

   Return 1 on success 0 on failure.  */

#define mod4(num) ( (num) & 3 )


int findJavaMagic (FILE *fileHandle)
{
  int offset=0;
  int foundMagic = 0;
  size_t rc;

  /* what were looking for */
  unsigned char magicInt[4] = {0xCA, 0xFE, 0xBA, 0xBE};
  /*the hex reads in decimal: 202 254 186 190 */

  /* a circular buffer indicating the last few bytes we read */
  unsigned char buffer[4] = {0};

  foundMagic = 0;
  while( !foundMagic ) {

      rc = fread(&buffer[offset], 1, 1, fileHandle);
      if ( !rc ) {

	/* Either this was not a java file or we were given a jar file
           and have already found the last java file in it.*/

	if ( feof(fileHandle) ) {
	  return 0;
	}
	
	if ( ferror(fileHandle) ) {
	  die ("Error reading character from file.\n");
	};

      }
      
      /* offset points to the most recent char we read so offest+1
         points to the oldest char we saved. */

      foundMagic = (
		    (magicInt[0] == buffer[mod4(offset+1)]) && 
		    (magicInt[1] == buffer[mod4(offset+2)]) && 
		    (magicInt[2] == buffer[mod4(offset+3)]) && 
		    (magicInt[3] == buffer[mod4(offset+0)]) && 
		    1
		    );	

      offset = mod4(offset+1);
      
    } /*end while*/

  return foundMagic;
}

#undef mod4


int
my_strcmp (const void *a, const void *b) {
char **a1; char **b1;
int ret;

a1 = (char **)a;
b1 = (char **)b;
ret = strcmp(*a1,*b1);
  return ret;
}

/* print the unique strings found in PRINT_TABLE and clear it out */

void 
print_table_flush(void) {
  int i;
  char *last_string;

  if (!SIZE_PRINT_TABLE) {
    return ;
  }

  /* The qsort line gives a warning on some unicies who insist that
     strcmp takes arguments of type pointers to void not the
     traditional pointers to char. */

  qsort( (void *) PRINT_TABLE, (size_t) SIZE_PRINT_TABLE, 
	 sizeof(char *), &my_strcmp);

  printf("%s",PRINT_TABLE[0]);
  last_string = PRINT_TABLE[0];
  PRINT_TABLE[0] = NULL;
  
  for (i = 1; i < SIZE_PRINT_TABLE; i++) {
    if ( strcmp(last_string, PRINT_TABLE[i]) ){
      printf("%s",PRINT_TABLE[i]);
      free(last_string);
      last_string = PRINT_TABLE[i];
    } else {
      free(PRINT_TABLE[i]);
    }
    PRINT_TABLE[i] = NULL;
  }
  
  free(last_string);
  SIZE_PRINT_TABLE = 0;
  return ; 
}


/* add an element to PRINT_TABLE for later printing to stdout.  We do
   not make a copy of the string so each string must be unique,
   (different calls must pass pointers to different areas of memory)
   and the string must not be freed anywhere else in the code and the
   string must be from memory which can be freed.*/

void 
print_table_add(char *str) {

  if (SIZE_PRINT_TABLE == MAX_PRINT_TABLE) {
    print_table_flush();
  }
  
  PRINT_TABLE[SIZE_PRINT_TABLE] = str;
  SIZE_PRINT_TABLE++;
  return ;
}


void 
print_list(char *in_string) {

  /* This function is no longer needed due to fixes in RPM's
     processing of dependencies.  Keep the code until I get a chance
     to use RPM3.0 personally */

  if (in_string) {
    printf("%s\n", in_string);
  }

/* 
   Old function did:

   Given a list separated by whitespace, put each element in the print
   table with an added "\n" */

 /*
  char *WhiteSpace_Set = "\t\v\n\r\f ";
  char *newEnd, *out_string;
  int copy_len;

  in_string += strspn(in_string, WhiteSpace_Set); 

  while (*in_string) {
    newEnd = strpbrk(in_string, WhiteSpace_Set);
    
    if  (newEnd) {
      copy_len = newEnd-in_string;
    } else {
      if (*in_string) {
	copy_len = strlen(in_string);
      } else {
	copy_len = 0;
      }
    }
    
    out_string = malloc(copy_len+10);
    if ( !out_string ) {
      outofmemory();
    }
    out_string[0]= '\0';

    if (copy_len) {
      strncat(out_string, in_string, copy_len);
      in_string+=copy_len;
      strcat(out_string, "\n");
      print_table_add(out_string);
    }

    in_string += strspn(in_string+copy_len, WhiteSpace_Set);
  }

 */
 return ;
}


/*  Print a properly formatted java class name, and returns the length
    of the class string .  Do not print \n here as we may wish to
    append the version number IFF we are printing the name of this classfile
    
    We also provide the class with the leaf node replaced with '*'.
    This would not be necessary if we only had to worry about java
    Class files.  However our parsing of jhtml files depends on this
    information.  This is deprecated since jhtml is deprecated and
    this method allows for very inaccurate dependencies.
    
    Also users may wish to refer to dependencies using star notation
    (though this would be less accurate then explicit dependencies
    since any leaf class will satify a star dependency).  */

char
*formatClassName(char *in_string, char terminator, 
		 char print_star)
{ 
  char *leaf_class=0, *out_string=0;
  char *ClassName_Break_Set=0;


  out_string = malloc(strlen(in_string) + 10);
  if ( !out_string ) {
    outofmemory();
  }
  out_string[0]= '\0';
  
  /*these characters end the current parse of the string in function
    formatClassName.*/
  
  ClassName_Break_Set = malloc(3);
  if ( !ClassName_Break_Set ) {
    outofmemory();
  }
  ClassName_Break_Set[0] = '/';
  /*terminator can be '\0' this must go after '/'*/
  ClassName_Break_Set[1] = terminator;
  ClassName_Break_Set[2] = '\0';
  
  if(ARG_RPMFORMAT) {
    strcat(out_string, "java(");
  }
  if (print_star) {
    /* print the path to the leaf class but do not print the leaf
       class, stop at the last '/' we fix this back below*/
    leaf_class = strrchr(in_string, '/');
    if (leaf_class) {
      leaf_class[0] = terminator;
    }
  }
  
  while (*in_string != terminator) {
    char *newEnd=0;
    int copy_len;

    /* handle the break_set */

    if (in_string[0] == '\0' ) {
      die("Classname does not terminate with: '%c', '%s'\n", 
	  terminator, in_string);
    } else {
      if (in_string[0] == '/' ) {
	/* convert '/' to '.' */
	strcat(out_string, ".");
	in_string++;
      }
    }

    newEnd = strpbrk(in_string, ClassName_Break_Set);

    if  (newEnd) {
      copy_len = newEnd-in_string;
    } else {
      if (terminator == '\0') {
	copy_len = strlen(in_string);
      } else {
	copy_len = 0;
      }
    }
    
    /* handle upto but not including the break_set*/
    if (copy_len) {
      strncat(out_string, in_string, copy_len);
      in_string+=copy_len;
    }

  } /*end while*/
  
  if (leaf_class) {
    /* print the star and fix the leaf class*/
    strcat(out_string, ".*"); 
    leaf_class[0] = '/';
  }
  if(ARG_RPMFORMAT) {
    strcat(out_string, ")");
  }

  strcat(out_string, "\n");
  free(ClassName_Break_Set);
  return out_string;
}


/* Parse out just the class names from a java type and moves the string
   pointer to the end of the string. */

void
dumpRefType(char *string)
{
  /* All class types start with a 'L' and and end with a ';'. We want
     everyting in between.  There might be more then one per string
     like (params for a method call) */

  string = strchr(string, 'L');
  while (string) {
    string++;
    print_table_add(formatClassName(string, ';', 0));
    string = strchr(string, ';');
    string = strchr(string, 'L');
  }
  
  return ;
}


/* Print out the classes referenced in the symbol table */

void
dumpRequires(symbolTable_t *symbolTable) {
  int tem;
  int ref = 0;

  for(tem=1; tem < symbolTable->poolSize; tem++ ) {

    /* dump all the classes in the const table. */
    ref = symbolTable->classRef[tem];
    if(ref) {
      char *string = symbolTable->stringList[ref];
      if( !*string ) {
	die("class num: %d, referenced string num: %d, "
	    "which is null.\n",
	    tem, ref);
      }
      if ( string[0] == '[' ) {
	/*
	  This is an array. We need to ingore 
	  strings like:
	       '[B'
	       '[[B'
	       '[[I'
	       
	  This hack leaves blank lines in the output 
	  when a string not containing a class is
	  sent to dumpRefType.
	*/
	string = strchr(string, 'L');
	if (string) {
	  dumpRefType(string);
	}
      } else {
	print_table_add(formatClassName(string, '\0', 0));
      }
    }
    
    /* dump all the references */
    ref = symbolTable->typeRef[tem];
    if (ref) {
      char *string = symbolTable->stringList[ref];
      if ( !*string ) {
	die("type num: %d, referenced string num: %d, "
	    "which is null.\n",
	    tem, ref);
      }
      /* this is a java type... parse out the class names */
      dumpRefType(string);
    } 
    
  } /*end for*/
  
  return ;
}


/* Read a java class files symbol table into memory. 
		- also  -
   Find the proper name of the current Java Class file.
   Print it regardless of: --provides | --requires
*/


void genSymbolTable (FILE *fileHandle, symbolTable_t *symbolTable)
{
  char ignore[10];
  int i=0;
  

  /* We are called just after fileHandle saw the magic number, seek a
     few bytes in to find the poolsize */

  my_fread(&ignore, 4, 1, fileHandle);

  my_fread(&(symbolTable->poolSize), 2, 1, fileHandle);

  /* new the tables */

  symbolTable->stringList = (char**) calloc(symbolTable->poolSize, 
					    sizeof(char*));
  if(!symbolTable->stringList){
    outofmemory();
  }
  
  symbolTable->classRef = (short*) calloc(symbolTable->poolSize,
					  sizeof(short*));
  if(!symbolTable->classRef){
    outofmemory();
  }
  
  symbolTable->typeRef = (short*) calloc(symbolTable->poolSize,
					 sizeof(short*));
  if(!symbolTable->typeRef){
    outofmemory();
  }
  
  /* zero 'em all out. */
  for(i=0; i < symbolTable->poolSize; i++) {
    symbolTable->stringList[i] = NULL;
    symbolTable->classRef[i] = 0;
    symbolTable->typeRef[i] = 0;
  }
  
  
  /* for the number of entries
       (it starts at 1)	 
  */

  for(i=1; i < symbolTable->poolSize; i++) {
    unsigned short type = 0;
    unsigned short value = 0;
    unsigned char tag = 0;

      /* read the type of this entry  */

      my_fread(&tag, 1, 1, fileHandle);
      switch(tag) {
	case 1: /* master string pool. */
	  {
	    /* record all these strings */
		char *someString;
		unsigned short length = 0;

		/* I am not sure if these strings must be null
		   terminated.  I termiante them to be safe. */

		my_fread(&length, 2, 1, fileHandle);
		someString = (char*) malloc(length+1);
		if(!someString){
		  outofmemory();
		}
		my_fread(someString, length, 1, fileHandle);
		someString[length]=0;
		symbolTable->stringList[i] = someString;
		
		if (ARG_KEYWORDS) {
		  char *ptr=0;

		  /* Each keyword can appear multiple times.  Don't
		    bother with datastructures to store these strings,
		    if we need to print it print it now.  */

		  /* it would be better if instead of printing the
		     strings "raw" I turn the space separated list
		     into a "\n" separated list*/
		  
		  if (ARG_REQUIRES) {
		    ptr = is_lower_equal(someString, "rpm_requires: ");
		    if(ptr){
		      print_list(ptr);
		    }
		  }
		  if (ARG_PROVIDES) {
		    ptr = is_lower_equal(someString, "rpm_provides: ");
		    if(ptr){
		      print_list(ptr);
		    }
		  }
		  /* I wish there was a good way to handle this
		  ptr = is_lower_equal(someString, "rpm_conflicts: ");
		  */
		  ptr = is_lower_equal(someString, "$revision: ");
		  if(ptr){
		    KEYWORD_REVISION=ptr;
		    /* terminate the string before " $" */
		    ptr = strchr(KEYWORD_REVISION, ' ');
		    if (ptr) {
		      *ptr = 0;
		    }
		  }
		  ptr = is_lower_equal(someString, "rpm_version: ");
		  if(ptr){
		    KEYWORD_VERSION=ptr;
		    /* terminate the string at first whitespace */
		    ptr = strchr(KEYWORD_VERSION, ' ');
		    if (ptr) {
		      *ptr = 0;
		    }
		  }
		  ptr = is_lower_equal(someString, "rpm_epoch: ");
		  if(ptr){
		    KEYWORD_EPOCH=ptr;
		    /* terminate the string at first whitespace */
		    ptr = strchr(KEYWORD_EPOCH, ' ');
		    if (ptr) {
		      *ptr = 0;
		    }
		  }
		}
		break;
	  }
      case 2:	/* unknow type!! */
	  die("Unknown type in constant table. "
	      "Entry: %d. \n", i);
	  break;
	case 3:	/* int */
	  my_fread(&ignore, 4, 1, fileHandle);
	  break;
	case 4: /* float */
	  my_fread(&ignore, 4, 1, fileHandle);
	  break;
	case 5: /* long (counts as 2) */
	  my_fread(&ignore, 8, 1, fileHandle);
	  i++;
	  break;
	case 6: /* double (counts as 2) */
	  my_fread(&ignore, 8, 1, fileHandle);
	  i++;
	  break;
	case 7: /* Class */
	  my_fread(&value, 2, 1, fileHandle);
	  /* record which const it's referencing */
	  check_range(value, symbolTable->poolSize);
	  symbolTable->classRef[i]=value;
	  break;
	case 8: /* String */
	  my_fread(&ignore, 2, 1, fileHandle);
	  break;
	case 9: /* field reference */
	  my_fread(&ignore, 4, 1, fileHandle);
	  break;
	case 10: /* method reference */
	  my_fread(&ignore, 4, 1, fileHandle);
	  break;
	case 11: /* interface method reference */
	  my_fread(&ignore, 4, 1, fileHandle);
	  break;
	case 12: /* constant name/type */
	  my_fread(&ignore, 2, 1, fileHandle);
	  my_fread(&type, 2, 1, fileHandle);
	  /* record the name, and the type it's referencing. */
	  check_range(type, symbolTable->poolSize);
	  symbolTable->typeRef[i]=type;
	  break;
	default:
	  die("Unknown tag type: %d.\n",
	      "Entry: %d. \n", tag, i);
	  break;
      }
  }

  return ;  
}
 

/* 
   Find the proper name of the current Java Class file.
   Print it regardless of: --provides | --requires
*/

void 
findClassName (FILE *fileHandle, symbolTable_t *symbolTable) {
  char ignore[10];
  unsigned short type = 0;
  unsigned short class = 0;
  char *out_string;
  char *newline;

  /* seek a little past the end of the table */
  
  my_fread(&ignore, 2, 1, fileHandle);
  
  /* read the name of this classfile */
  
  my_fread(&type, 2, 1, fileHandle);
  class = symbolTable->classRef[type];
  if( !class ||
      !symbolTable->stringList[class] ) {
      die("Couln't find class: %d, provided by file.\n", class);
  }
  CLASS_NAME=symbolTable->stringList[class];

  out_string = formatClassName(symbolTable->stringList[class], '\0', 0);

  {
    int len = 10;

    if (out_string) {
      len += strlen(out_string);
    }
    if (KEYWORD_EPOCH) {
      len += strlen(KEYWORD_EPOCH);
    }
    if (KEYWORD_VERSION) {
      len += strlen(KEYWORD_VERSION);
    }
    if (KEYWORD_REVISION) {
      len += strlen(KEYWORD_REVISION);
    }
    
    out_string = realloc(out_string, len );
  }

  if (!out_string){
    outofmemory();
  }
  
  if( KEYWORD_VERSION || KEYWORD_REVISION ){
    /* It is easier to remove the extra new line here in one place
       then to try and add a newline every where that formatClassName
       is called */
    char *newline;

    /* I am not using rpm 3.0 yet so I need both the dependencies with
       and without the version numbers, when I upgrade I will remove
       this block (with copy_string) and change the "=" to " = " ten
       lines down.*/
    {
      char *copy_string;
      copy_string = (char*) malloc(strlen(out_string));
      if (!copy_string){
	outofmemory();
      }
      copy_string = strcpy(copy_string, out_string);
      print_table_add(copy_string);
    }

    newline = strrchr(out_string, '\n');
    if (newline) {
      newline[0] = '\0';
    }
    strcat(out_string, " = ");
    if(KEYWORD_EPOCH){
      strcat(out_string, KEYWORD_EPOCH);
      strcat(out_string, ":");
    }
    if(KEYWORD_VERSION){
      strcat(out_string, KEYWORD_VERSION);
    } else {
      strcat(out_string, KEYWORD_REVISION);
    }
    strcat(out_string, "\n");
  }
  
  print_table_add(out_string);
  out_string=NULL;

  /* Provide the star version of this class for jhtml
     dependencies. This option is deprecated since jhtml is
     deprecated. */
  
  if (ARG_STARPROV) {
    out_string = formatClassName(symbolTable->stringList[class], '\0', 1);
    print_table_add(out_string);
  }
  
  return ;  
}





void freeSymbolTable (symbolTable_t *symbolTable)
{  
  int i=0;

  for(i=1; i < symbolTable->poolSize; i++) {
    if( symbolTable->stringList[i] ) {
      free(symbolTable->stringList[i]);
      symbolTable->stringList[i] = 0;
    }
  }
  
  free(symbolTable->stringList);
  symbolTable->stringList=0;
  
  free(symbolTable->classRef);
  symbolTable->classRef=0;
  
  free(symbolTable->typeRef);
  symbolTable->typeRef=0;
  
  free(symbolTable);
  symbolTable=0;

  return ;
}


/* process each file, 
   must be called directly after finding 
   the magic number.
*/

void processJavaFile (FILE *fileHandle) {
  symbolTable_t symbolTable= {0};
  
  genSymbolTable(fileHandle, &symbolTable);
  findClassName(fileHandle, &symbolTable);
  
  if(ARG_REQUIRES) {
    dumpRequires(&symbolTable);
  }

  freeSymbolTable(&symbolTable);

  return ;

}


int
main(int argc, char **argv)
{
  FILE *fileHandle;
  int i = 0;
  int rc = 0;
  int foundMagic=0;

  PROGRAM_NAME=argv[0];
  
  if(argv[1] == NULL) {
    usage();
  }
  
  /* parse arguments which are not filenames*/
  
  for (i = 1; argv[i] != NULL; i++) {
    
    if (0) {
      /* 
	 First entry a dummy to get the 
	 other entries to align correctly
      */
      ;
    } else if ( !strcmp("-p",argv[i]) || !strcmp("--provides",argv[i]) ) {
      ARG_PROVIDES = 1;
    } else if ( !strcmp("-r",argv[i]) || !strcmp("--requires",argv[i]) ) {
      ARG_REQUIRES = 1;
    } else if ( !strcmp("-h",argv[i]) || !strcmp("--help",argv[i]) ||
		!strcmp("-?",argv[i]) ) {
	        
      usage();
    } else if ( !strcmp("-F",argv[i]) || !strcmp("--rpmformat",argv[i]) ) {
      ARG_RPMFORMAT=1;
    } else if ( !strcmp("-k",argv[i]) || !strcmp("--keywords",argv[i]) ) {
      ARG_KEYWORDS=1;
    } else if ( !strcmp("-s",argv[i]) || !strcmp("--starprov",argv[i]) ) {
      ARG_STARPROV=1;
    } else if ( !strcmp("--",argv[i]) ) {
      i++;
      break;      
    } else {
      /* we do not recognize the argument, must be a filename*/
      break;
    }
  } /*end for arguments which are not filenames*/
  
  /* check arguments for consistancy */

  if ( !ARG_PROVIDES && !ARG_REQUIRES ) {
    die ("Must specify either --provides or --requires.\n");
  }
  
  if ( ARG_PROVIDES && ARG_REQUIRES ) {
    die ("Can not specify both --provides and --requires.\n");
  }
  
  if ( ARG_REQUIRES && ARG_STARPROV) {
    die ("Can not specify both --requires and --starpov.\n");
  }
  
  if(argv[i] == NULL) {
    die ("Must specify Java class files.\n");
  }
  
  /* parse arguments which are filenames.  */

  for ( /*null initializer*/; argv[i] != NULL; i++) {
    
    /*open the correct file and process it*/
    
    if ( !strcmp("-", argv[i]) ) {
      /* use stdin, might be a jar file */
      fileHandle = stdin;
      FILE_NAME = "<stdin>";

      foundMagic = findJavaMagic(fileHandle);      
      while (foundMagic) {	
	processJavaFile(fileHandle);
	foundMagic = findJavaMagic(fileHandle);
      } 
    } else {
      /* Open a disk file*/
      fileHandle = fopen(argv[i], "r");
      if( fileHandle == 0 ) {
	die ("Could not open file: %s.\n", argv[i]);
      }
      fileHandle = fileHandle;
      FILE_NAME = argv[i];      

      foundMagic = findJavaMagic(fileHandle);      
      if (foundMagic) {	
	processJavaFile(fileHandle);
      }
    }

    rc = fclose(fileHandle);
    if( rc ) {
      die ("Could not close file: %s.\n", FILE_NAME);
    }
    CLASS_NAME=0;    
  } /*end parsing arguments which are filenames*/
  
  print_table_flush();
  return 0;
}
