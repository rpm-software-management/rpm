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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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
char *PROGRAMNAME=0;

/*name of the current class file*/
char *FILENAME=0;

/*the name of the last class file seen*/
char *CLASSNAME=0;

/*arguments chosen*/
int ARG_PROVIDES=0;
int ARG_REQUIRES=0;
int ARG_RPMFORMAT=0;
int ARG_KEYWORDS=0;
int ARG_STARPROV=0;

/*keywords found in class file*/
char *KEYWORD_VERSION=0;


/*--------- declare all functions ---------*/

void usage (void);
void outofmemory(void);
void die(char *format, ...);
size_t my_fread(void *ptr, size_t size, size_t nitems, FILE *stream);
void check_range(short value, short poolSize);
char *is_lower_equal (char *string, char *pattern);
int findJavaMagic (FILE *fileHandle);
int dumpClassName(char *pSomeString, char terminator, char print_star);
void dumpRefType(char *pSomeString);
void dumpRequires(symbolTable_t *symbolTable);
void genSymbolTable (FILE *fileHandle, symbolTable_t *symbolTable);
void findClassName  (FILE *fileHandle, symbolTable_t *symbolTable);
void freeSymbolTable (symbolTable_t *symbolTable);

/*--------- functions ---------*/

void
usage (void)
{
  printf("NAME:\n\tjavadeps - Examine Java class files and\n"
	 "\t\treturn information about their dependencies.\n\n");
  printf("USAGE:\n");
  printf("\t javadeps { --provides | --requires } \n"
	 "\t\t   [--rpmformat] [--keywords] \n"
	 "\t\t     [--] classfile-name ... \n\n"
	 "\t javadeps [--help]\n\n");
  printf("\n\n");
  printf("DESCRIPTION:\n\n");
  printf("List the dependencies or the fully qualified class names, of the \n"
	 "classfiles listed on the command line. \n");
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
	 "and, if found, are used to change the dependencies accordingly.\n"
	 "RPM_Provides and RPM_Requires may appear multiple times in the \n"
	 "string table indicating that the dependency is the union of all\n"
	 "entries.\n\n"
	 "Keyword List:\n\n"
	 "'$Revision: '     This RCS/CVS compatible keyword is assumed to \n"
	 "                  contain the version number of the class file \n"
	 "                  it is found in.  Care should be taken with this\n" 
	 "                  option as RPM's notion of which version is later\n"
	 "                  may not corrispond with your own, especially\n"
	 "                  if you use branches.  This keyword only effects\n"
	 "                  the output of --provides. \n"
	 "                   (rpm needs modifications to support version \n"
	 "                    numbers in find-provides)\n\n"
	 "'RPM_Provides: '  This string lists additinal capabilites provided\n"
	 "                  by the java class.  This keyword only effects \n"
	 "                  the output of --provides.\n\n"
	 "'RPM_Requires: '  This string lists additional requirements of\n"
	 "                  the java class.  This keyword only effects the \n"
	 "                  output of --requires.\n\n");
  printf("EXAMPLES: \n\n"
	 "\tjavadeps --requires -- filename.class\n\n"
	 "\tjavadeps --provides -- filename.class\n\n"
	 "\tjavadeps --help\n\n"
	 "\n"
	 "\tjavadeps --requires --rpmformat --keywords -- filename.class\n\n"
	 "\tjavadeps --requires -- filename1.class filename2.class\n\n"
	 "\tcat filename2.class | javadeps --requires -- filename1.class -\n\n"
	 "\tunzip -p filename.jar | javadeps --requires -- - \n\n");
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

  char  *newformat, *newmsg;
  va_list ap;

  if ( !(newformat = malloc(1024)) || !(newmsg = malloc(1024)) )
    outofmemory();

  /* Rewrite format line, to include additional information.  The
     format line we chose depends on how much information is availible
     at the time of the error.  Display the maximum ammount of
     information. */

  /* notice the FILENAME is useless for jar files.  We would want to
     print the name of the classfile which caused the error.  That
     is hard since we only know that name when we are done parsing
     the file, and most errors will occur before that.*/

  va_start(ap, format);
  
  if ( (!FILENAME) ) {

    sprintf (newformat, "\n%s: %s",
	     PROGRAMNAME, format);

  } else if ( (FILENAME) && (!CLASSNAME) ) {
    
    sprintf (newformat, "\n%s: Java classfile: %s, %s",
	     PROGRAMNAME, FILENAME, format);
    
  } else if (CLASSNAME) {
    sprintf (newformat, "\n%s: Java classfile: %s, classname: %s, %s",
	     PROGRAMNAME, FILENAME, CLASSNAME, format);
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
  return ;
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
   fileread error.

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

int
dumpClassName(char *pSomeString, char terminator, 
	      char print_star)
{ 
  char *leaf_class=0;
  int chars =0;
  /* 
     For each char in the class name format it and print it to stdout.
     No class terminator is written in this function.
     */

  if(ARG_RPMFORMAT) {
    printf("java(");
  }

  if (print_star) {
    /*do not print the leaf class*/
    leaf_class = strrchr(pSomeString, '/');
    if (leaf_class) {
      leaf_class[0] = terminator;
    }
  }
  
  while (1) {
	if (*pSomeString == terminator) {
	/* normal exit from loop when end of class reached */
	  if (leaf_class) {
	    /* print the star and fix the leaf class*/
	    printf(".*"); 
	    leaf_class[0] = '.';
	  }
	  if(ARG_RPMFORMAT) {
	    printf(")");
	  }
	  return chars;
	} else if (*pSomeString == '\0' ) {
	/* fatal error, exit from loop when end of string reached and
           terminator is not \0 */
	  die("Classname does not terminate with: '%c'\n", 
	      terminator);
	} else if (*pSomeString == '/' ) {
	  /* convert '/' to '.' */
	  printf(".");
	} else {
	  /* print this char and keep going, */
	  printf("%c",*pSomeString);
	} /*end switch*/
      pSomeString++;
      chars++;
      } /*end while*/
  /*not reached*/
  return chars;
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
    string = (string) +  dumpClassName(string, ';', 0);
    printf("\n");
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
	dumpClassName(string, '\0', 0);
	printf("\n");
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
      /* this is a java type... parr out the class names */
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
  int i;
  

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

		  if (ARG_REQUIRES) {
		    ptr = is_lower_equal(someString, "rpm_requires: ");
		    if(ptr){
		      printf(ptr);
		    }
		  }
		  if (ARG_PROVIDES) {
		    ptr = is_lower_equal(someString, "rpm_provides: ");
		    if(ptr){
		      printf(ptr);
		    }
		  }
		  /* I wish there was a good way to handle this
		  ptr = is_lower_equal(someString, "rpm_conflicts: ");
		  */
		  ptr = is_lower_equal(someString, "$revision: ");
		  if(ptr){
		    KEYWORD_VERSION=ptr;
		    /* terminate the string before " $" */
		    ptr = strchr(KEYWORD_VERSION, ' ');
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
  char *string;
  unsigned short type = 0;
  unsigned short class = 0;
  
  /* seek a little past the end of the table */
  
  my_fread(&ignore, 2, 1, fileHandle);
  
  /* read the name of this classfile */
  
  my_fread(&type, 2, 1, fileHandle);
  class = symbolTable->classRef[type];
  if( !class ||
      !symbolTable->stringList[class] ) {
      die("Couln't find class: %d, provided by file.\n", class);
  }
  CLASSNAME=symbolTable->stringList[class];
  dumpClassName(symbolTable->stringList[class], '\0', 0);

  if(KEYWORD_VERSION){
    /*  
	rpm does not currenly allow version numbers via find-provides
	so as a hack dump the name twice, once with the version and
	once without.  Eventually we will not need this extra dump.
    */
    printf("\n"); dumpClassName(symbolTable->stringList[class], '\0', 0);

    printf("=%s", KEYWORD_VERSION);
  }
  printf("\n");

  /* Provide the star version of this class for jhtml
     dependencies. This option is deprecated since jhtml is
     deprecated. */

  if (ARG_STARPROV) {
    dumpClassName(symbolTable->stringList[class], '\0', 1);
  }
  printf("\n");   

  return ;  
}





void freeSymbolTable (symbolTable_t *symbolTable)
{  
  int i;

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

  PROGRAMNAME=argv[0];
  
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

  for (0; argv[i] != NULL; i++) {
    
    /*open the correct file and process it*/
    
    if ( !strcmp("-", argv[i]) ) {
      /* use stdin, might be a jar file */
      fileHandle = stdin;
      FILENAME = "<stdin>";

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
      FILENAME = argv[i];      

      foundMagic = findJavaMagic(fileHandle);      
      if (foundMagic) {	
	processJavaFile(fileHandle);
      }
    }

    rc = fclose(fileHandle);
    if( rc ) {
      die ("Could not close file: %s.\n", FILENAME);
    }
    CLASSNAME=0;    
  } /*end parsing arguments which are filenames*/
  
  return 0;
}
