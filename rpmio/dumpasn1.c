/* ASN.1 object dumping code, copyright Peter Gutmann
   <pgut001@cs.auckland.ac.nz>, based on ASN.1 dump program by David Kemp
   <dpkemp@missi.ncsc.mil>, with contributions from various people including
   Matthew Hamrick <hamrick@rsa.com>, Bruno Couillard
   <bcouillard@chrysalis-its.com>, Hallvard Furuseth
   <h.b.furuseth@usit.uio.no>, Geoff Thorpe <geoff@raas.co.nz>, David Boyce
   <d.boyce@isode.com>, John Hughes <john.hughes@entegrity.com>, Life is
   hard, and then you die <ronald@trustpoint.com>, and several other people
   whose names I've misplaced.

   Available from http://www.cs.auckland.ac.nz/~pgut001/dumpasn1.c.
   Last updated 21 November 2000.

   This version of dumpasn1 requires a config file dumpasn1.cfg to be present
   in the same location as the program itself or in a standard directory
   where binaries live (it will run without it but will display a warning
   message, you can configure the path either by hardcoding it in or using an
   environment variable as explained further down).  The config file is
   available from http://www.cs.auckland.ac.nz/~pgut001/dumpasn1.cfg.

   This code assumes that the input data is binary, having come from a MIME-
   aware mailer or been piped through a decoding utility if the original
   format used base64 encoding.  Bruno Couillard has created a modified
   version which will read raw base64-encoded data (ie without any MIME
   encapsulation or other headers) directly, at the expense of being somewhat
   non-portable.  Alternatively, you can use utilities like uudeview (which
   will strip virtually any kind of encoding, MIME, PEM, PGP, whatever) to
   recover the binary original.

   You can use this code in whatever way you want, as long as you don't try
   to claim you wrote it.

   Editing notes: Tabs to 4, phasers to stun */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Useful defines */

#ifndef TRUE
  #define FALSE	0
  #define TRUE	( !FALSE )
#endif /* TRUE */

/* SunOS 4.x doesn't define seek codes or exit codes or FILENAME_MAX (it does
   define _POSIX_MAX_PATH, but in funny locations and to different values
   depending on which include file you use).  Some OS's also define
   FILENAME_MAX to silly values (eg 14 bytes), so we replace it with a more
   sensible setting if necessary */

#ifndef SEEK_SET
  #define SEEK_SET	0
  #define SEEK_CUR	2
#endif /* No fseek() codes defined */
#ifndef EXIT_FAILURE
  #define EXIT_FAILURE	1
  #define EXIT_SUCCESS	( !EXIT_FAILURE )
#endif /* No exit() codes defined */
#ifndef FILENAME_MAX
  #define FILENAME_MAX	512
#else
  #if FILENAME_MAX < 128
	#undef FILENAME_MAX
	#define FILENAME_MAX	512
  #endif /* FILENAME_MAX < 128 */
#endif /* FILENAME_MAX */

/* Under Windows we can do special-case handling for things like BMPStrings */

#if ( defined( _WINDOWS ) || defined( WIN32 ) || defined( _WIN32 ) || \
	  defined( __WIN32__ ) )
  #define __WIN32__
#endif /* Win32 */

/* Some OS's don't define the min() macro */

#ifndef min
  #define min(a,b)		( ( a ) < ( b ) ? ( a ) : ( b ) )
#endif /* !min */

/* The level of recursion can get scary for deeply-nested structures so we
   use a larger-than-normal stack under DOS */

#ifdef  __TURBOC__
  extern unsigned _stklen = 16384;
#endif /* __TURBOC__ */

/* When we dump a nested data object encapsulated within a larger object, the
   length is initially set to a magic value which is adjusted to the actual
   length once we start parsing the object */

#define LENGTH_MAGIC	177545L

/* Tag classes */

#define CLASS_MASK		0xC0	/* Bits 8 and 7 */
#define UNIVERSAL		0x00	/* 0 = Universal (defined by ITU X.680) */
#define APPLICATION		0x40	/* 1 = Application */
#define CONTEXT			0x80	/* 2 = Context-specific */
#define PRIVATE			0xC0	/* 3 = Private */

/* Encoding type */

#define FORM_MASK		0x20	/* Bit 6 */
#define PRIMITIVE		0x00	/* 0 = primitive */
#define CONSTRUCTED		0x20	/* 1 = constructed */

/* Universal tags */

#define TAG_MASK		0x1F	/* Bits 5 - 1 */
#define EOC				0x00	/*  0: End-of-contents octets */
#define BOOLEAN			0x01	/*  1: Boolean */
#define INTEGER			0x02	/*  2: Integer */
#define BITSTRING		0x03	/*  2: Bit string */
#define OCTETSTRING		0x04	/*  4: Byte string */
#define NULLTAG			0x05	/*  5: NULL */
#define OID				0x06	/*  6: Object Identifier */
#define OBJDESCRIPTOR	0x07	/*  7: Object Descriptor */
#define EXTERNAL		0x08	/*  8: External */
#define REAL			0x09	/*  9: Real */
#define ENUMERATED		0x0A	/* 10: Enumerated */
#define EMBEDDED_PDV	0x0B	/* 11: Embedded Presentation Data Value */
#define UTF8STRING		0x0C	/* 12: UTF8 string */
#define SEQUENCE		0x10	/* 16: Sequence/sequence of */
#define SET				0x11	/* 17: Set/set of */
#define NUMERICSTRING	0x12	/* 18: Numeric string */
#define PRINTABLESTRING	0x13	/* 19: Printable string (ASCII subset) */
#define T61STRING		0x14	/* 20: T61/Teletex string */
#define VIDEOTEXSTRING	0x15	/* 21: Videotex string */
#define IA5STRING		0x16	/* 22: IA5/ASCII string */
#define UTCTIME			0x17	/* 23: UTC time */
#define GENERALIZEDTIME	0x18	/* 24: Generalized time */
#define GRAPHICSTRING	0x19	/* 25: Graphic string */
#define VISIBLESTRING	0x1A	/* 26: Visible string (ASCII subset) */
#define GENERALSTRING	0x1B	/* 27: General string */
#define UNIVERSALSTRING	0x1C	/* 28: Universal string */
#define BMPSTRING		0x1E	/* 30: Basic Multilingual Plane/Unicode string */

/* Length encoding */

#define LEN_XTND  0x80		/* Indefinite or long form */
#define LEN_MASK  0x7F		/* Bits 7 - 1 */

/* Various special-case operations to perform on strings */

typedef enum {
	STR_NONE,				/* No special handling */
	STR_UTCTIME,			/* Check it's UTCTime */
	STR_PRINTABLE,			/* Check it's a PrintableString */
	STR_IA5,				/* Check it's an IA5String */
	STR_BMP					/* Read and display string as Unicode */
	} STR_OPTION;

/* Structure to hold info on an ASN.1 item */

typedef struct {
	int id;						/* Identifier */
	int tag;					/* Tag */
	long length;				/* Data length */
	int indefinite;				/* Item has indefinite length */
	int headerSize;				/* Size of tag+length */
	unsigned char header[ 8 ];	/* Tag+length data */
	} ASN1_ITEM;

/* Config options */

static int printDots = FALSE;		/* Whether to print dots to align columns */
static int doPure = FALSE;			/* Print data without LHS info column */
static int doDumpHeader = FALSE;	/* Dump tag+len in hex (level = 0, 1, 2) */
static int extraOIDinfo = FALSE;	/* Print extra information about OIDs */
static int doHexValues = FALSE;		/* Display size, offset in hex not dec.*/
static int useStdin = FALSE;		/* Take input from stdin */
static int zeroLengthAllowed = FALSE;/* Zero-length items allowed */
static int dumpText = FALSE;		/* Dump text alongside hex data */
static int printAllData = FALSE;	/* Whether to print all data in long blocks */
static int checkEncaps = TRUE;		/* Print encaps.data in BIT/OCTET STRINGs */

/* Error and warning information */

static int noErrors = 0;			/* Number of errors found */
static int noWarnings = 0;			/* Number of warnings */

/* Position in the input stream */

static int fPos = 0;				/* Absolute position in data */

/* The output stream */

static FILE *output;				/* Output stream */

/* Information on an ASN.1 Object Identifier */

#define MAX_OID_SIZE	32

typedef struct tagOIDINFO {
	struct tagOIDINFO *next;		/* Next item in list */
	char oid[ MAX_OID_SIZE ], *comment, *description;
	int oidLength;					/* Name, rank, serial number */
	int warn;						/* Whether to warn if OID encountered */
	} OIDINFO;

static OIDINFO *oidList = NULL;

/* If the config file isn't present in the current directory, we search the
   following paths (this is needed for Unix with dumpasn1 somewhere in the
   path, since this doesn't set up argv[0] to the full path).  Anything
   beginning with a '$' uses the appropriate environment variable */

#define CONFIG_NAME		"dumpasn1.cfg"

static const char *configPaths[] = {
	/* Unix absolute paths */
	"/bin/", "/usr/bin/", "/usr/local/bin/",

	/* Windoze absolute paths.  Usually things are on C:, but older NT setups
	   are easier to do on D: if the initial copy is done to C: */
	"c:\\dos\\", "d:\\dos\\", "c:\\windows\\", "d:\\windows\\",
	"c:\\winnt\\", "d:\\winnt\\",

	/* It's my program, I'm allowed to hardcode in strange paths which noone
	   else uses */
	"$HOME/BIN/", "c:\\program files\\bin\\",

	/* Unix environment-based paths */
	"$HOME/", "$HOME/bin/",

	/* General environment-based paths */
	"$DUMPASN1_PATH/",

	NULL
	};

#define isEnvTerminator( c )	\
	( ( ( c ) == '/' ) || ( ( c ) == '.' ) || ( ( c ) == '$' ) || \
	  ( ( c ) == '\0' ) || ( ( c ) == '~' ) )

/****************************************************************************
*																			*
*					Object Identification/Description Routines				*
*																			*
****************************************************************************/

/* Return descriptive strings for universal tags */

char *idstr( const int tagID )
	{
	switch( tagID )
		{
		case EOC:
			return( "End-of-contents octets" );
		case BOOLEAN:
			return( "BOOLEAN" );
		case INTEGER:
			return( "INTEGER" );
		case BITSTRING:
			return( "BIT STRING" );
		case OCTETSTRING:
			return( "OCTET STRING" );
		case NULLTAG:
			return( "NULL" );
		case OID:
			return( "OBJECT IDENTIFIER" );
		case OBJDESCRIPTOR:
			return( "ObjectDescriptor" );
		case EXTERNAL:
			return( "EXTERNAL" );
		case REAL:
			return( "REAL" );
		case ENUMERATED:
			return( "ENUMERATED" );
		case EMBEDDED_PDV:
			return( "EMBEDDED PDV" );
		case UTF8STRING:
			return( "UTF8String" );
		case SEQUENCE:
			return( "SEQUENCE" );
		case SET:
			return( "SET" );
		case NUMERICSTRING:
			return( "NumericString" );
		case PRINTABLESTRING:
			return( "PrintableString" );
		case T61STRING:
			return( "TeletexString" );
		case VIDEOTEXSTRING:
			return( "VideotexString" );
		case IA5STRING:
			return( "IA5String" );
		case UTCTIME:
			return( "UTCTime" );
		case GENERALIZEDTIME:
			return( "GeneralizedTime" );
		case GRAPHICSTRING:
			return( "GraphicString" );
		case VISIBLESTRING:
			return( "VisibleString" );
		case GENERALSTRING:
			return( "GeneralString" );
		case UNIVERSALSTRING:
			return( "UniversalString" );
		case BMPSTRING:
			return( "BMPString" );
		default:
			return( "Unknown (Reserved)" );
		}
	}

/* Return information on an object identifier */

static OIDINFO *getOIDinfo( char *oid, const int oidLength )
	{
	OIDINFO *oidPtr;

	memset( oid + oidLength, 0, 2 );
	for( oidPtr = oidList; oidPtr != NULL; oidPtr = oidPtr->next )
		if( oidLength == oidPtr->oidLength - 2 && \
			!memcmp( oidPtr->oid + 2, oid, oidLength ) )
			return( oidPtr );

	return( NULL );
	}

/* Add an OID attribute */

static int addAttribute( char **buffer, char *attribute )
	{
	if( ( *buffer = ( char * ) malloc( strlen( attribute ) + 1 ) ) == NULL )
		{
		puts( "Out of memory." );
		return( FALSE );
		}
	strcpy( *buffer, attribute );
	return( TRUE );
	}

/* Table to identify valid string chars (taken from cryptlib) */

#define P	1						/* PrintableString */
#define I	2						/* IA5String */
#define PI	3						/* IA5String and PrintableString */

static int charFlags[] = {
	/* 00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F */
		0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	/* 10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F */
		0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	/*		!	"	#	$	%	&	'	(	)	*	+	,	-	.	/ */
	   PI,	I,	I,	I,	I,	I,	I, PI, PI, PI,	I, PI, PI, PI, PI, PI,
	/*	0	1	2	3	4	5	6	7	8	9	:	;	<	=	>	? */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I, PI,	I, PI,
	/*	@	A	B	C	D	E	F	G	H	I	J	K	L	M	N	O */
		I, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,
	/*	P	Q	R	S	T	U	V	W	X	Y	Z	[	\	]	^ _ */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I,	I,	I,	I,
	/*	`	a	b	c	d	e	f	g	h	i	j	k	l	m	n	o */
		I, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,
	/*	p	q	r	s	t	u	v	w	x	y	z	{	|	}	~  DL */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I,	I,	I,	0
	};

static int isPrintable( int ch )
	{
	if( ch >= 128 || !( charFlags[ ch ] & P ) )
		return( FALSE );
	return( TRUE );
	}

static int isIA5( int ch )
	{
	if( ch >= 128 || !( charFlags[ ch ] & I ) )
		return( FALSE );
	return( TRUE );
	}

/****************************************************************************
*																			*
*							Config File Read Routines						*
*																			*
****************************************************************************/

/* Files coming from DOS/Windows systems may have a ^Z (the CP/M EOF char)
   at the end, so we need to filter this out */

#define CPM_EOF	0x1A		/* ^Z = CPM EOF char */

/* The maximum input line length */

#define MAX_LINESIZE	512

/* Read a line of text from the config file */

static int lineNo;

static int readLine( FILE *file, char *buffer )
	{
	int bufCount = 0, ch;

	/* Skip whitespace */
	while( ( ( ch = getc( file ) ) == ' ' || ch == '\t' ) && !feof( file ) );

	/* Get a line into the buffer */
	while( ch != '\r' && ch != '\n' && ch != CPM_EOF && !feof( file ) )
		{
		/* Check for an illegal char in the data.  Note that we don't just
		   check for chars with high bits set because these are legal in
		   non-ASCII strings */
		if( ( ch & 0x7F ) < ' ' )
			{
			printf( "Bad character '%c' in config file line %d.\n",
					ch, lineNo );
			return( FALSE );
			}

		/* Check to see if it's a comment line */
		if( ch == '#' && !bufCount )
			{
			/* Skip comment section and trailing whitespace */
			while( ch != '\r' && ch != '\n' && ch != CPM_EOF && !feof( file ) )
				ch = getc( file );
			break;
			}

		/* Make sure the line is of the correct length */
		if( bufCount > MAX_LINESIZE )
			{
			printf( "Config file line %d too long.\n", lineNo );
			return( FALSE );
			}
		else
			if( ch )	/* Can happen if we read a binary file */
				buffer[ bufCount++ ] = ch;

		/* Get next character */
		ch = getc( file );
		}

	/* If we've just passed a CR, check for a following LF */
	if( ch == '\r' )
		if( ( ch = getc( file ) ) != '\n' )
			ungetc( ch, file );

	/* Skip trailing whitespace and add der terminador */
	while( bufCount > 0 &&
		   ( ( ch = buffer[ bufCount - 1 ] ) == ' ' || ch == '\t' ) )
		bufCount--;
	buffer[ bufCount ] = '\0';

	/* Handle special-case of ^Z if file came off an MSDOS system */
	if( ch == CPM_EOF )
		while( !feof( file ) )
			/* Keep going until we hit the true EOF (or some sort of error) */
			ch = getc( file );

	return( ferror( file ) ? FALSE : TRUE );
	}

/* Process an OID specified as space-separated hex digits */

static int processHexOID( OIDINFO *oidInfo, char *string )
	{
	int value, index = 0;

	while( *string && index < MAX_OID_SIZE - 1 )
		{
		if( sscanf( string, "%x", &value ) != 1 || value > 255 )
			{
			printf( "Invalid hex value in config file line %d.\n", lineNo );
			return( FALSE );
			}
		oidInfo->oid[ index++ ] = value;
		string += 2;
		if( *string && *string++ != ' ' )
			{
			printf( "Invalid hex string in config file line %d.\n", lineNo );
			return( FALSE );
			}
		}
	oidInfo->oid[ index ] = 0;
	oidInfo->oidLength = index;
	if( index >= MAX_OID_SIZE - 1 )
		{
		printf( "OID value in config file line %d too long.\n", lineNo );
		return( FALSE );
		}
	return( TRUE );
	}

/* Read a config file */

static int readConfig( const char *path, const int isDefaultConfig )
	{
	OIDINFO dummyOID = { NULL, "Dummy", "Dummy", "Dummy", 1 }, *oidPtr;
	FILE *file;
	char buffer[ MAX_LINESIZE ];
	int status;

	/* Try and open the config file */
	if( ( file = fopen( path, "rb" ) ) == NULL )
		{
		/* If we can't open the default config file, issue a warning but
		   continue anyway */
		if( isDefaultConfig )
			{
			puts( "Cannot open config file 'dumpasn1.cfg', which should be in the same" );
			puts( "directory as the dumpasn1 program.  Operation will continue without" );
			puts( "the ability to display Object Identifier information." );
			puts( "" );
			puts( "If the config file is located elsewhere, you can set the environment" );
			puts( "variable DUMPASN1_CFG to the path to the file." );
			return( TRUE );
			}

		printf( "Cannot open config file '%s'.\n", path );
		return( FALSE );
		}

	/* Add the new config entries at the appropriate point in the OID list */
	if( oidList == NULL )
		oidPtr = &dummyOID;
	else
		for( oidPtr = oidList; oidPtr->next != NULL; oidPtr = oidPtr->next );

	/* Read each line in the config file */
	lineNo = 1;
	while( ( status = readLine( file, buffer ) ) == TRUE && !feof( file ) )
		{
		/* If it's a comment line, skip it */
		if( !*buffer )
			{
			lineNo++;
			continue;
			}

		/* Check for an attribute tag */
		if( !strncmp( buffer, "OID = ", 6 ) )
			{
			/* Make sure all the required attributes for the current OID are
			   present */
			if( oidPtr->description == NULL )
				{
				printf( "OID ending on config file line %d has no "
						"description attribute.\n", lineNo - 1 );
				return( FALSE );
				}

			/* Allocate storage for the new OID */
			if( ( oidPtr->next = ( struct tagOIDINFO * ) \
								 malloc( sizeof( OIDINFO ) ) ) == NULL )
				{
				puts( "Out of memory." );
				return( FALSE );
				}
			oidPtr = oidPtr->next;
			if( oidList == NULL )
				oidList = oidPtr;
			memset( oidPtr, 0, sizeof( OIDINFO ) );

			/* Add the new OID */
			if( !processHexOID( oidPtr, buffer + 6 ) )
				return( FALSE );
			}
		else if( !strncmp( buffer, "Description = ", 14 ) )
			{
			if( oidPtr->description != NULL )
				{
				printf( "Duplicate OID description in config file line %d.\n",
						lineNo );
				return( FALSE );
				}
			if( !addAttribute( &oidPtr->description, buffer + 14 ) )
				return( FALSE );
			}
		else if( !strncmp( buffer, "Comment = ", 10 ) )
			{
			if( oidPtr->comment != NULL )
				{
				printf( "Duplicate OID comment in config file line %d.\n",
						lineNo );
				return( FALSE );
				}
			if( !addAttribute( &oidPtr->comment, buffer + 10 ) )
				return( FALSE );
			}
		else if( !strncmp( buffer, "Warning", 7 ) )
			{
			if( oidPtr->warn )
				{
				printf( "Duplicate OID warning in config file line %d.\n",
						lineNo );
				return( FALSE );
				}
			oidPtr->warn = TRUE;
			}
		else
			{
			printf( "Unrecognised attribute '%s', line %d.\n", buffer,
					lineNo );
			return( FALSE );
			}

		lineNo++;
		}
	fclose( file );

	return( status );
	}

/* Check for the existence of a config file path */

static int testConfigPath( const char *path )
	{
	FILE *file;

	/* Try and open the config file */
	if( ( file = fopen( path, "rb" ) ) == NULL )
		return( FALSE );
	fclose( file );

	return( TRUE );
	}

/* Build a config path by substituting environment strings for $NAMEs */

static void buildConfigPath( char *path, const char *pathTemplate )
	{
	char pathBuffer[ FILENAME_MAX ], newPath[ FILENAME_MAX ];
	int pathLen, pathPos = 0, newPathPos = 0;

	/* Add the config file name at the end */
	strcpy( pathBuffer, pathTemplate );
	strcat( pathBuffer, CONFIG_NAME );
	pathLen = strlen( pathBuffer );

	while( pathPos < pathLen )
		{
		char *strPtr;
		int substringSize;

		/* Find the next $ and copy the data before it to the new path */
		if( ( strPtr = strstr( pathBuffer + pathPos, "$" ) ) != NULL )
			substringSize = ( int ) ( ( strPtr - pathBuffer ) - pathPos );
		else
			substringSize = pathLen - pathPos;
		if( substringSize > 0 )
			memcpy( newPath + newPathPos, pathBuffer + pathPos,
					substringSize );
		newPathPos += substringSize;
		pathPos += substringSize;

		/* Get the environment string for the $NAME */
		if( strPtr != NULL )
			{
			char envName[ MAX_LINESIZE ], *envString;
			int i;

			/* Skip the '$', find the end of the $NAME, and copy the name
			   into an internal buffer */
			pathPos++;	/* Skip the $ */
			for( i = 0; !isEnvTerminator( pathBuffer[ pathPos + i ] ); i++ );
			memcpy( envName, pathBuffer + pathPos, i );
			envName[ i ] = '\0';

			/* Get the env.string and copy it over */
			if( ( envString = getenv( envName ) ) != NULL )
				{
				const int envStrLen = strlen( envString );

				if( newPathPos + envStrLen < FILENAME_MAX - 2 )
					{
					memcpy( newPath + newPathPos, envString, envStrLen );
					newPathPos += envStrLen;
					}
				}
			pathPos += i;
			}
		}
	newPath[ newPathPos ] = '\0';	/* Add der terminador */

	/* Copy the new path to the output */
	strcpy( path, newPath );
	}

/* Read the global config file */

static int readGlobalConfig( const char *path )
	{
	char buffer[ FILENAME_MAX ], *namePos;
	int i;

	/* First, try and find the config file in the same directory as the
	   executable.  This requires that argv[0] be set up properly, which
	   isn't the case if Unix search paths are being used, and seems to be
	   pretty broken under Windows */
	namePos = strstr( path, "dumpasn1" );
	if( namePos == NULL )
		namePos = strstr( path, "DUMPASN1" );
	if( strlen( path ) < FILENAME_MAX - 13 && namePos != NULL )
		{
		strcpy( buffer, path );
		strcpy( buffer + ( int ) ( namePos - ( char * ) path ), CONFIG_NAME );
		if( testConfigPath( buffer ) )
			return( readConfig( buffer, TRUE ) );
		}

	/* Now try each of the possible absolute locations for the config file */
	for( i = 0; configPaths[ i ] != NULL; i++ )
		{
		buildConfigPath( buffer, configPaths[ i ] );
		if( testConfigPath( buffer ) )
			return( readConfig( buffer, TRUE ) );
		}

	/* Default out to just the config name (which should fail as it was the
	   first entry in configPaths[]).  readConfig() will display the
	   appropriate warning */
	return( readConfig( CONFIG_NAME, TRUE ) );
	}

/****************************************************************************
*																			*
*							Output/Formatting Routines						*
*																			*
****************************************************************************/

/* Indent a string by the appropriate amount */

static void doIndent( const int level )
	{
	int i;

	for( i = 0; i < level; i++ )
		fprintf( output, ( printDots ) ? ". " : "  " );
	}

/* Complain about an error in the ASN.1 object */

static void complain( const char *message, const int level )
	{
	if( !doPure )
		fprintf( output, "            : " );
	doIndent( level + 1 );
	fprintf( output, "Error: %s.\n", message );
	noErrors++;
	}

/* Dump data as a string of hex digits up to a maximum of 128 bytes */

static void dumpHex( FILE *inFile, long length, int level, int isInteger )
	{
	const int lineLength = ( dumpText ) ? 8 : 16;
	char printable[ 9 ];
	long noBytes = length;
	int zeroPadded = FALSE, warnPadding = FALSE, warnNegative = isInteger;
	int maxLevel = ( doPure ) ? 15 : 8, i;

	if( noBytes > 128 && !printAllData )
		noBytes = 128;	/* Only output a maximum of 128 bytes */
	if( level > maxLevel )
		level = maxLevel;	/* Make sure we don't go off edge of screen */
	printable[ 8 ] = printable[ 0 ] = '\0';
	for( i = 0; i < noBytes; i++ )
		{
		int ch;

		if( !( i % lineLength ) )
			{
			if( dumpText )
				{
				/* If we're dumping text alongside the hex data, print the
				   accumulated text string */
				fputs( "    ", output );
				fputs( printable, output );
				}
			fputc( '\n', output );
			if( !doPure )
				fprintf( output, "            : " );
			doIndent( level + 1 );
			}
		ch = getc( inFile );
		fprintf( output, "%s%02X", i % lineLength ? " " : "", ch );
		printable[ i % 8 ] = ( ch >= ' ' && ch < 127 ) ? ch : '.';
		fPos++;

		/* If we need to check for negative values and zero padding, check
		   this now */
		if( !i )
			{
			if( !ch )
				zeroPadded = TRUE;
			if( !( ch & 0x80 ) )
				warnNegative = FALSE;
			}
		if( i == 1 && zeroPadded && ch < 0x80 )
			warnPadding = TRUE;
		}
	if( dumpText )
		{
		/* Print any remaining text */
		i %= lineLength;
		printable[ i ] = '\0';
		while( i < lineLength )
			{
			fprintf( output, "   " );
			i++;
			}
		fputs( "    ", output );
		fputs( printable, output );
		}
	if( length > 128 && !printAllData )
		{
		length -= 128;
		fputc( '\n', output );
		if( !doPure )
			fprintf( output, "            : " );
		doIndent( level + 5 );
		fprintf( output, "[ Another %ld bytes skipped ]", length );
		if( useStdin )
			{
			while( length-- )
				getc( inFile );
			}
		else
			fseek( inFile, length, SEEK_CUR );
		fPos += length;
		}
	fputs( "\n", output );

	if( isInteger )
		{
		if( warnPadding )
			complain( "Integer has non-DER encoding", level );
		if( warnNegative )
			complain( "Integer has a negative value", level );
		}
	}

/* Dump a bitstring, reversing the bits into the standard order in the
   process */

static void dumpBitString( FILE *inFile, const int length, const int unused,
						   const int level )
	{
	unsigned int bitString = 0, currentBitMask = 0x80, remainderMask = 0xFF;
	int bitFlag, value = 0, noBits, bitNo = -1, i;
	char *errorStr = NULL;

	if( unused < 0 || unused > 7 )
		complain( "Invalid number of unused bits", level );
	noBits = ( length * 8 ) - unused;

	/* ASN.1 bitstrings start at bit 0, so we need to reverse the order of
	   the bits */
	if( length )
		{
		bitString = fgetc( inFile );
		fPos++;
		}
	for( i = noBits - 8; i > 0; i -= 8 )
		{
		bitString = ( bitString << 8 ) | fgetc( inFile );
		currentBitMask <<= 8;
		remainderMask = ( remainderMask << 8 ) | 0xFF;
		fPos++;
		}
	for( i = 0, bitFlag = 1; i < noBits; i++ )
		{
		if( bitString & currentBitMask )
			value |= bitFlag;
		if( !( bitString & remainderMask ) )
			/* The last valid bit should be a one bit */
			errorStr = "Spurious zero bits in bitstring";
		bitFlag <<= 1;
		bitString <<= 1;
		}
	if( ( remainderMask << noBits ) & value )
		/* There shouldn't be any bits set after the last valid one */
		errorStr = "Spurious one bits in bitstring";

	/* Now that it's in the right order, dump it.  If there's only one
	   bit set (which is often the case for bit flags) we also print the
	   bit number to save users having to count the zeroes to figure out
	   which flag is set */
	fputc( '\n', output );
	if( !doPure )
		fprintf( output, "            : " );
	doIndent( level + 1 );
	fputc( '\'', output );
	currentBitMask = 1 << ( noBits - 1 );
	for( i = 0; i < noBits; i++ )
		{
		if( value & currentBitMask )
			{
			bitNo = ( bitNo == -1 ) ? ( noBits - 1 ) - i : -2;
			fputc( '1', output );
			}
		else
			fputc( '0', output );
		currentBitMask >>= 1;
		}
	if( bitNo >= 0 )
		fprintf( output, "'B (bit %d)\n", bitNo );
	else
		fputs( "'B\n", output );

	if( errorStr != NULL )
		complain( errorStr, level );
	}

/* Display data as a text string up to a maximum of 240 characters (8 lines
   of 48 chars to match the hex limit of 8 lines of 16 bytes) with special
   treatement for control characters and other odd things which can turn up
   in BMPString and UniversalString types.

   If the string is less than 40 chars in length, we try to print it on the
   same line as the rest of the text (even if it wraps), otherwise we break
   it up into 48-char chunks in a somewhat less nice text-dump format */

static void displayString( FILE *inFile, long length, int level,
						   STR_OPTION strOption )
	{
	long noBytes = ( length > 384 ) ? 384 : length;
	int lineLength = 48;
	int maxLevel = ( doPure ) ? 15 : 8, firstTime = TRUE, i;
	int warnIA5 = FALSE, warnPrintable = FALSE, warnUTC = FALSE;
	int warnBMP = FALSE;

	if( strOption == STR_UTCTIME && length != 13 )
		warnUTC = TRUE;
	if( length <= 40 )
		fprintf( output, " '" );		/* Print string on same line */
	if( level > maxLevel )
		level = maxLevel;	/* Make sure we don't go off edge of screen */
	for( i = 0; i < noBytes; i++ )
		{
		int ch;

		/* If the string is longer than 40 chars, break it up into multiple
		   sections */
		if( length > 40 && !( i % lineLength ) )
			{
			if( !firstTime )
				fputc( '\'', output );
			fputc( '\n', output );
			if( !doPure )
				fprintf( output, "            : " );
			doIndent( level + 1 );
			fputc( '\'', output );
			firstTime = FALSE;
			}
		ch = getc( inFile );
#ifdef __WIN32__
		if( strOption == STR_BMP )
			{
			if( i == noBytes - 1 && ( noBytes & 1 ) )
				/* Odd-length BMP string, complain */
				warnBMP = TRUE;
			else
				{
				wchar_t wCh = ( ch << 8 ) | getc( inFile );
				unsigned char outBuf[ 8 ];
				int outLen;

				/* Attempting to display Unicode characters is pretty hit and
				   miss, and if it fails nothing is displayed.  To try and
				   detect this we use wcstombs() to see if anything can be
				   displayed, if it can't we drop back to trying to display
				   the data as non-Unicode */
				outLen = wcstombs( outBuf, &wCh, 1 );
				if( outLen < 1 )
					{
					/* Can't be displayed as Unicode, fall back to
					   displaying it as normal text */
					ungetc( wCh & 0xFF, inFile );
					}
				else
					{
					lineLength++;
					i++;	/* We've read two characters for a wchar_t */
					wprintf( L"%c", wCh );
					fPos += 2;
					continue;
					}
				}
			}
#endif /* __WIN32__ */
		if( strOption == STR_PRINTABLE || strOption == STR_IA5 )
			{
			if( strOption == STR_PRINTABLE && !isPrintable( ch ) )
				warnPrintable = TRUE;
			if( strOption == STR_IA5 && !isIA5( ch ) )
				warnIA5 = TRUE;
			if( ch < ' ' || ch >= 0x7F )
				ch = '.';		/* Convert non-ASCII to placeholders */
			}
		else
			if( strOption == STR_UTCTIME )
				{
				if( !isdigit( ch ) && ch != 'Z' )
					{
					warnUTC = TRUE;
					ch = '.';	/* Convert non-numeric to placeholders */
					}
				}
			else
				if( ( ch & 0x7F ) < ' ' || ch == 0xFF )
					ch = '.';	/* Convert control chars to placeholders */
		fputc( ch, output );
		fPos++;
		}
	if( length > 384 )
		{
		length -= 384;
		fprintf( output, "'\n" );
		if( !doPure )
			fprintf( output, "            : " );
		doIndent( level + 5 );
		fprintf( output, "[ Another %ld characters skipped ]", length );
		fPos += length;
		while( length-- )
			{
			int ch = getc( inFile );

			if( strOption == STR_PRINTABLE && !isPrintable( ch ) )
				warnPrintable = TRUE;
			if( strOption == STR_IA5 && !isIA5( ch ) )
				warnIA5 = TRUE;
			}
		}
	else
		fputc( '\'', output );
	fputc( '\n', output );

	/* Display any problems we encountered */
	if( warnPrintable )
		complain( "PrintableString contains illegal character(s)", level );
	if( warnIA5 )
		complain( "IA5String contains illegal character(s)", level );
	if( warnUTC )
		complain( "UTCTime is encoded incorrectly", level );
	if( warnBMP )
		complain( "BMPString has missing final byte/half character", level );
	}

/****************************************************************************
*																			*
*								ASN.1 Parsing Routines						*
*																			*
****************************************************************************/

/* Get an integer value */

static long getValue( FILE *inFile, const long length )
	{
	long value;
	char ch;
	int i;

	ch = getc( inFile );
	value = ch;
	for( i = 0; i < length - 1; i++ )
		value = ( value << 8 ) | getc( inFile );
	fPos += length;

	return( value );
	}

/* Get an ASN.1 objects tag and length */

int getItem( FILE *inFile, ASN1_ITEM *item )
	{
	int tag, length, index = 0;

	memset( item, 0, sizeof( ASN1_ITEM ) );
	item->indefinite = FALSE;
	tag = item->header[ index++ ] = fgetc( inFile );
	item->id = tag & ~TAG_MASK;
	tag &= TAG_MASK;
	if( tag == TAG_MASK )
		{
		int value;

		/* Long tag encoded as sequence of 7-bit values.  This doesn't try to
		   handle tags > INT_MAX, it'd be pretty peculiar ASN.1 if it had to
		   use tags this large */
		tag = 0;
		do
			{
			value = fgetc( inFile );
			tag = ( tag << 7 ) | ( value & 0x7F );
			item->header[ index++ ] = value;
			fPos++;
			}
		while( value & LEN_XTND && !feof( inFile ) );
		}
	item->tag = tag;
	if( feof( inFile ) )
		{
		fPos++;
		return( FALSE );
		}
	fPos += 2;			/* Tag + length */
	length = item->header[ index++ ] = fgetc( inFile );
	item->headerSize = index;
	if( length & LEN_XTND )
		{
		int i;

		length &= LEN_MASK;
		if( length > 4 )
			/* Impossible length value, probably because we've run into
			   the weeds */
			return( -1 );
		item->headerSize += length;
		item->length = 0;
		if( !length )
			item->indefinite = TRUE;
		for( i = 0; i < length; i++ )
			{
			int ch = fgetc( inFile );

			item->length = ( item->length << 8 ) | ch;
			item->header[ i + index ] = ch;
			}
		fPos += length;
		}
	else
		item->length = length;

	return( TRUE );
	}

/* Check whether a BIT STRING or OCTET STRING encapsulates another object */

static int checkEncapsulate( FILE *inFile, const int tag, const int length )
	{
	ASN1_ITEM nestedItem;
	const int currentPos = fPos;
	int diffPos;

	/* If we're not looking for encapsulated objects, return */
	if( !checkEncaps )
		return( FALSE );

#if 1
	/* Read the details of the next item in the input stream */
	getItem( inFile, &nestedItem );
	diffPos = fPos - currentPos;
	fPos = currentPos;
	fseek( inFile, -diffPos, SEEK_CUR );

	/* If it fits exactly within the current item and has a valid-looking
	   tag, treat it as nested data */
	if( ( ( nestedItem.id & CLASS_MASK ) == UNIVERSAL || \
		  ( nestedItem.id & CLASS_MASK ) == CONTEXT ) && \
		( nestedItem.tag > 0 && nestedItem.tag <= 0x31 ) && \
		nestedItem.length == length - diffPos )
		return( TRUE );
#else
	/* Older code which used heuristics but was actually less accurate than
	   the above code */
	int ch;

	/* Get the first character and see if it's an INTEGER or SEQUENCE */
	ch = getc( inFile );
	ungetc( ch, inFile );
	if( ch == INTEGER || ch == ( SEQUENCE | CONSTRUCTED ) )
		return( TRUE );

	/* All sorts of weird things get bundled up in octet strings in
	   certificate extensions */
	if( tag == OCTETSTRING && ch == BITSTRING )
		return( TRUE );

	/* If we're looking for all sorts of things which might be encapsulated,
	   check for these as well.  At the moment we only check for a small
	   number of possibilities, this list will probably change as more
	   oddities are discovered, the idea is to keep the amount of burrowing
	   we do to a minimum in order to reduce problems with false positives */
	if( level > 1 && tag == OCTETSTRING )
		{
		int length;

		if( ch == IA5STRING )
			/* Verisign extensions */
			return( TRUE );

		/* For the following possibilities we have to look ahead a bit
		   further and check the length as well */
		getc( inFile );
		length = getc( inFile );
		fseek( inFile, -2, SEEK_CUR );
		if( ( ch == OID && length < 9 ) || \
			( ch == ENUMERATED && length == 1 ) || \
			( ch == GENERALIZEDTIME && length == 15 ) )
			/* CRL per-entry extensions */
			return( TRUE );
		}
#endif /* 0 */

	return( FALSE );
	}

/* Check whether a zero-length item is OK */

int zeroLengthOK( const ASN1_ITEM *item )
	{
	/* If we can't recognise the type from the tag, reject it */
	if( ( item->id & CLASS_MASK ) != UNIVERSAL )
		return( FALSE );

	/* The following types are zero-length by definition */
	if( item->tag == EOC || item->tag == NULLTAG )
		return( TRUE );

	/* A real with a value of zero has zero length */
	if( item->tag == REAL )
		return( TRUE );

	/* Everything after this point requires input from the user to say that
	   zero-length data is OK (usually it's not, so we flag it as a
	   problem) */
	if( !zeroLengthAllowed )
		return( FALSE );

	/* String types can have zero length except for the Unrestricted
	   Character String type ([UNIVERSAL 29]) which has to have at least one
	   octet for the CH-A/CH-B index */
	if( item->tag == OCTETSTRING || item->tag == NUMERICSTRING || \
		item->tag == PRINTABLESTRING || item->tag == T61STRING || \
		item->tag == VIDEOTEXSTRING || item->tag == VISIBLESTRING || \
		item->tag == IA5STRING || item->tag == GRAPHICSTRING || \
		item->tag == GENERALSTRING || item->tag == UNIVERSALSTRING || \
		item->tag == BMPSTRING || item->tag == UTF8STRING || \
		item->tag == OBJDESCRIPTOR )
		return( TRUE );

	/* SEQUENCE and SET can be zero if there are absent optional/default
	   components */
	if( item->tag == SEQUENCE || item->tag == SET )
		return( TRUE );

	return( FALSE );
	}

/* Check whether the next item looks like text */

static int looksLikeText( FILE *inFile, const int length )
	{
	char buffer[ 16 ];
	int sampleLength = min( length, 16 ), i;

	/* If the sample size is too small, don't try anything */
	if( sampleLength < 4 )
		return( FALSE );

	/* Check for ASCII-looking text */
	sampleLength = fread( buffer, 1, sampleLength, inFile );
	fseek( inFile, -sampleLength, SEEK_CUR );
	for( i = 0; i < sampleLength; i++ )
		{
		if( !( i & 1 ) && !buffer[ i ] )
			/* If even bytes are zero, it could be a BMPString */
			continue;
		if( buffer[ i ] < 0x20 || buffer[ i ] > 0x7E )
			return( FALSE );
		}

	/* It looks like a text string */
	return( TRUE);
	}

/* Dump the header bytes for an object, useful for vgrepping the original
   object from a hex dump */

static void dumpHeader( FILE *inFile, const ASN1_ITEM *item )
	{
	int extraLen = 24 - item->headerSize, i;

	/* Dump the tag and length bytes */
	if( !doPure )
		fprintf( output, "    " );
	fprintf( output, "<%02X", *item->header );
	for( i = 1; i < item->headerSize; i++ )
		fprintf( output, " %02X", item->header[ i ] );

	/* If we're asked for more, dump enough extra data to make up 24 bytes.
	   This is somewhat ugly since it assumes we can seek backwards over the
	   data, which means it won't always work on streams */
	if( extraLen > 0 && doDumpHeader > 1 )
		{
		/* Make sure we don't print too much data.  This doesn't work for
		   indefinite-length data, we don't try and guess the length with
		   this since it involves picking apart what we're printing */
		if( extraLen > item->length && !item->indefinite )
			extraLen = ( int ) item->length;

		for( i = 0; i < extraLen; i++ )
			{
			int ch = fgetc( inFile );

			if( feof( inFile ) )
				extraLen = i;	/* Exit loop and get fseek() correct */
			else
				fprintf( output, " %02X", ch );
			}
		fseek( inFile, -extraLen, SEEK_CUR );
		}

	fputs( ">\n", output );
	}

/* Print a constructed ASN.1 object */

int printAsn1( FILE *inFile, const int level, long length, const int isIndefinite );

static void printConstructed( FILE *inFile, int level, const ASN1_ITEM *item )
	{
	int result;

	/* Special case for zero-length objects */
	if( !item->length && !item->indefinite )
		{
		fputs( " {}\n", output );
		return;
		}

	fputs( " {\n", output );
	result = printAsn1( inFile, level + 1, item->length, item->indefinite );
	if( result )
		{
		fprintf( output, "Error: Inconsistent object length, %d byte%s "
				 "difference.\n", result, ( result > 1 ) ? "s" : "" );
		noErrors++;
		}
	if( !doPure )
		fprintf( output, "            : " );
	fprintf( output, ( printDots ) ? ". " : "  " );
	doIndent( level );
	fputs( "}\n", output );
	}

/* Print a single ASN.1 object */

void printASN1object( FILE *inFile, ASN1_ITEM *item, int level )
	{
	OIDINFO *oidInfo;
	char buffer[ MAX_OID_SIZE ];
	long value;
	int x, y;

	if( ( item->id & CLASS_MASK ) != UNIVERSAL )
		{
		static const char *const classtext[] =
			{ "UNIVERSAL ", "APPLICATION ", "", "PRIVATE " };

		/* Print the object type */
		fprintf( output, "[%s%d]",
				 classtext[ ( item->id & CLASS_MASK ) >> 6 ], item->tag );

		/* Perform a sanity check */
		if( ( item->tag != NULLTAG ) && ( item->length < 0 ) )
			{
			int i;

			fprintf( stderr, "\nError: Object has bad length field, tag = %02X, "
					 "length = %lX, value =", item->tag, item->length );
			fprintf( stderr, "<%02X", *item->header );
			for( i = 1; i < item->headerSize; i++ )
				fprintf( stderr, " %02X", item->header[ i ] );
			fputs( ">.\n", stderr );
			exit( EXIT_FAILURE );
			}

		if( !item->length && !item->indefinite )
			{
			fputc( '\n', output );
			complain( "Object has zero length", level );
			return;
			}

		/* If it's constructed, print the various fields in it */
		if( ( item->id & FORM_MASK ) == CONSTRUCTED )
			{
			printConstructed( inFile, level, item );
			return;
			}

		/* It's primitive, if it's a seekable stream try and determine
		   whether it's text so we can display it as such */
		if( !useStdin && looksLikeText( inFile, item->length ) )
			{
			/* It looks like a text string, dump it as text */
			displayString( inFile, item->length, level, STR_NONE );
			return;
			}

		/* This could be anything, dump it as hex data */
		dumpHex( inFile, item->length, level, FALSE );

		return;
		}

	/* Print the object type */
	fprintf( output, "%s", idstr( item->tag ) );

	/* Perform a sanity check */
	if( ( item->tag != NULLTAG ) && ( item->length < 0 ) )
		{
		int i;

		fprintf( stderr, "\nError: Object has bad length field, tag = %02X, "
				 "length = %lX, value =", item->tag, item->length );
		fprintf( stderr, "<%02X", *item->header );
		for( i = 1; i < item->headerSize; i++ )
			fprintf( stderr, " %02X", item->header[ i ] );
		fputs( ">.\n", stderr );
		exit( EXIT_FAILURE );
		}

	/* If it's constructed, print the various fields in it */
	if( ( item->id & FORM_MASK ) == CONSTRUCTED )
		{
		printConstructed( inFile, level, item );
		return;
		}

	/* It's primitive */
	if( !item->length && !zeroLengthOK( item ) )
		{
		fputc( '\n', output );
		complain( "Object has zero length", level );
		return;
		}
	switch( item->tag )
		{
		case BOOLEAN:
			x = getc( inFile );
			fprintf( output, " %s\n", x ? "TRUE" : "FALSE" );
			if( x != 0 && x != 0xFF )
				complain( "BOOLEAN has non-DER encoding", level );
			fPos++;
			break;

		case INTEGER:
		case ENUMERATED:
			if( item->length > 4 )
				dumpHex( inFile, item->length, level, TRUE );
			else
				{
				value = getValue( inFile, item->length );
				fprintf( output, " %ld\n", value );
				if( value < 0 )
					complain( "Integer has a negative value", level );
				}
			break;

		case BITSTRING:
			fprintf( output, " %d unused bits", x = getc( inFile ) );
			fPos++;
			if( !--item->length && !x )
				{
				fputc( '\n', output );
				complain( "Object has zero length", level );
				return;
				}
			if( item->length <= sizeof( int ) )
				{
				/* It's short enough to be a bit flag, dump it as a sequence
				   of bits */
				dumpBitString( inFile, ( int ) item->length, x, level );
				break;
				}
		case OCTETSTRING:
			if( checkEncapsulate( inFile, item->tag, item->length ) )
				{
				/* It's something encapsulated inside the string, print it as
				   a constructed item */
				fprintf( output, ", encapsulates" );
				printConstructed( inFile, level + 1, item );
				break;
				}
			if( !useStdin && !dumpText && \
				looksLikeText( inFile, item->length ) )
				{
				/* If we'd be doing a straight hex dump and it looks like
				   encapsulated text, display it as such */
				displayString( inFile, item->length, level, STR_NONE );
				return;
				}
			dumpHex( inFile, item->length, level, FALSE );
			break;

		case OID:
			/* Hierarchical Object Identifier: The first two levels are
			   encoded into one byte, since the root level has only 3 nodes
			   (40*x + y).  However if x = joint-iso-itu-t(2) then y may be
			   > 39, so we have to add special-case handling for this */
			if( item->length > MAX_OID_SIZE )
				{
				fprintf( stderr, "\nError: Object identifier length %ld too "
						 "large.\n", item->length );
				exit( EXIT_FAILURE );
				}
			fread( buffer, 1, ( size_t ) item->length, inFile );
			fPos += item->length;
			if( ( oidInfo = getOIDinfo( buffer, ( int ) item->length ) ) != NULL )
				{
				int lhsSize = ( doPure ) ? 0 : 14;

				/* Check if LHS status info + indent + "OID " string + oid
				   name will wrap */
				if( lhsSize + ( level * 2 ) + 18 + strlen( oidInfo->description ) >= 80 )
					{
					fputc( '\n', output );
					if( !doPure )
						fprintf( output, "            : " );
					doIndent( level + 1 );
					}
				else
					fputc( ' ', output );
				fprintf( output, "%s\n", oidInfo->description );

				/* Display extra comments about the OID if required */
				if( extraOIDinfo && oidInfo->comment != NULL )
					{
					if( !doPure )
						fprintf( output, "            : " );
					doIndent( level + 1 );
					fprintf( output, "(%s)\n", oidInfo->comment );
					}

				/* If there's a warning associated with this OID, remember
				   that there was a problem */
				if( oidInfo->warn )
					noWarnings++;

				break;
				}

			/* Pick apart the OID */
			x = ( unsigned char ) buffer[ 0 ] / 40;
			y = ( unsigned char ) buffer[ 0 ] % 40;
			if( x > 2 )
				{
				/* Handle special case for large y if x = 2 */
				y += ( x - 2 ) * 40;
				x = 2;
				}
			fprintf( output, " '%d %d", x, y );
			value = 0;
			for( x = 1; x < item->length; x++ )
				{
				value = ( value << 7 ) | ( buffer[ x ] & 0x7F );
				if( !( buffer[ x ] & 0x80 ) )
					{
					fprintf( output, " %ld", value );
					value = 0;
					}
				}
			fprintf( output, "'\n" );
			break;

		case EOC:
		case NULLTAG:
			fputc( '\n', output );
			break;

		case OBJDESCRIPTOR:
		case GENERALIZEDTIME:
		case GRAPHICSTRING:
		case VISIBLESTRING:
		case GENERALSTRING:
		case UNIVERSALSTRING:
		case NUMERICSTRING:
		case T61STRING:
		case VIDEOTEXSTRING:
		case UTF8STRING:
			displayString( inFile, item->length, level, STR_NONE );
			break;
		case PRINTABLESTRING:
			displayString( inFile, item->length, level, STR_PRINTABLE );
			break;
		case BMPSTRING:
			displayString( inFile, item->length, level, STR_BMP );
			break;
		case UTCTIME:
			displayString( inFile, item->length, level, STR_UTCTIME );
			break;
		case IA5STRING:
			displayString( inFile, item->length, level, STR_IA5 );
			break;

		default:
			fputc( '\n', output );
			if( !doPure )
				fprintf( output, "            : " );
			doIndent( level + 1 );
			fprintf( output, "Unrecognised primitive, hex value is:");
			dumpHex( inFile, item->length, level, FALSE );
			noErrors++;		/* Treat it as an error */
		}
	}

/* Print a complex ASN.1 object */

int printAsn1( FILE *inFile, const int level, long length,
			   const int isIndefinite )
	{
	ASN1_ITEM item;
	long lastPos = fPos;
	int seenEOC = FALSE, status;

	/* Special-case for zero-length objects */
	if( !length && !isIndefinite )
		return( 0 );

	while( ( status = getItem( inFile, &item ) ) > 0 )
		{
		/* If the length isn't known and the item has a definite length, set
		   the length to the items length */
		if( length == LENGTH_MAGIC && !item.indefinite )
			length = item.headerSize + item.length;

		/* Dump the header as hex data if requested */
		if( doDumpHeader )
			dumpHeader( inFile, &item );

		/* Print offset into buffer, tag, and length */
		if( !doPure )
			if( item.indefinite )
				fprintf( output, ( doHexValues ) ? "%04lX %02X NDEF: " :
						 "%4ld %02X NDEF: ", lastPos, item.id | item.tag );
			else
				if( ( item.id | item.tag ) == EOC )
					seenEOC = TRUE;
				else
					fprintf( output, ( doHexValues ) ? "%04lX %02X %4lX: " :
							 "%4ld %02X %4ld: ", lastPos, item.id | item.tag,
							 item.length );

		/* Print details on the item */
		if( !seenEOC )
			{
			doIndent( level );
			printASN1object( inFile, &item, level );
			}

		/* If it was an indefinite-length object (no length was ever set) and
		   we've come back to the top level, exit */
		if( length == LENGTH_MAGIC )
			return( 0 );

		length -= fPos - lastPos;
		lastPos = fPos;
		if( isIndefinite )
			{
			if( seenEOC )
				return( 0 );
			}
		else
			if( length <= 0 )
				{
				if( length < 0 )
					return( ( int ) -length );
				return( 0 );
				}
			else
				if( length == 1 )
					{
					const int ch = fgetc( inFile );

					/* No object can be one byte long, try and recover.  This
					   only works sometimes because it can be caused by
					   spurious data in an OCTET STRING hole or an incorrect
					   length encoding.  The following workaround tries to
					   recover from spurious data by skipping the byte if
					   it's zero or a non-basic-ASN.1 tag, but keeping it if
					   it could be valid ASN.1 */
					if( ch && ch <= 0x31 )
						ungetc( ch, inFile );
					else
						{
						fPos++;
						return( 1 );
						}
					}
		}
	if( status == -1 )
		{
		fprintf( stderr, "\nError: Invalid data encountered at position "
				 "%d.\n", fPos );
		exit( EXIT_FAILURE );
		}

	/* If we see an EOF and there's supposed to be more data present,
	   complain */
	if( length && length != LENGTH_MAGIC )
		{
		fprintf( output, "Error: Inconsistent object length, %ld byte%s "
				 "difference.\n", length, ( length > 1 ) ? "s" : "" );
		noErrors++;
		}
	return( 0 );
	}

/* Show usage and exit */

void usageExit( void )
	{
	puts( "DumpASN1 - ASN.1 object dump/syntax check program." );
	puts( "Copyright Peter Gutmann 1997 - 2000.  Last updated 21 November 2000." );
	puts( "" );
	puts( "Usage: dumpasn1 [-acdefhlpsxz] <file>" );
	puts( "       - = Take input from stdin (some options may not work properly)" );
	puts( "       -<number> = Start <number> bytes into the file" );
	puts( "       -- = End of arg list" );
	puts( "       -a = Print all data in long data blocks, not just the first 128 bytes" );
	puts( "       -c<file> = Read Object Identifier info from alternate config file" );
	puts( "            (values will override equivalents in global config file)" );
	puts( "       -d = Print dots to show column alignment" );
	puts( "       -e = Don't print encapsulated data inside OCTET/BIT STRINGs" );
	puts( "       -f<file> = Dump object at offset -<number> to file (allows data to be" );
	puts( "            extracted from encapsulating objects)" );
	puts( "       -h = Hex dump object header (tag+length) before the decoded output" );
	puts( "       -hh = Same as -h but display more of the object as hex data" );
	puts( "       -l = Long format, display extra info about Object Identifiers" );
	puts( "       -p = Pure ASN.1 output without encoding information" );
	puts( "       -s = Syntax check only, don't dump ASN.1 structures" );
	puts( "       -t = Display text values next to hex dump of data" );
	puts( "       -x = Display size and offset in hex not decimal" );
	puts( "       -z = Allow zero-length items" );
	puts( "" );
	puts( "Warnings generated by deprecated OIDs require the use of '-l' to be displayed." );
	puts( "Program return code is the number of errors found or EXIT_SUCCESS." );
	exit( EXIT_FAILURE );
	}

int main( int argc, char *argv[] )
	{
	FILE *inFile, *outFile = NULL;
	char *pathPtr = argv[ 0 ];
	long offset = 0;
	int moreArgs = TRUE, doCheckOnly = FALSE;

	/* Skip the program name */
	argv++; argc--;

	/* Display usage if no args given */
	if( argc < 1 )
		usageExit();
	output = stdout;	/* Needs to be assigned at runtime */

	/* Check for arguments */
	while( argc && *argv[ 0 ] == '-' && moreArgs )
		{
		char *argPtr = argv[ 0 ] + 1;

		if( !*argPtr )
			useStdin = TRUE;
		while( *argPtr )
			{
			if( isdigit( *argPtr ) )
				{
				offset = atol( argPtr );
				break;
				}
			switch( toupper( *argPtr ) )
				{
				case '-':
					moreArgs = FALSE;	/* GNU-style end-of-args flag */
					break;

				case 'A':
					printAllData = TRUE;
					break;

				case 'C':
					if( !readConfig( argPtr + 1, FALSE ) )
						exit( EXIT_FAILURE );
					while( argPtr[ 1 ] )
						argPtr++;	/* Skip rest of arg */
					break;

				case 'D':
					printDots = TRUE;
					break;

				case 'E':
					checkEncaps = FALSE;
					break;

				case 'F':
					if( ( outFile = fopen( argPtr + 1, "wb" ) ) == NULL )
						{
						perror( argPtr + 1 );
						exit( EXIT_FAILURE );
						}
					while( argPtr[ 1 ] )
						argPtr++;	/* Skip rest of arg */
					break;

				case 'L':
					extraOIDinfo = TRUE;
					break;

				case 'H':
					doDumpHeader++;
					break;

				case 'P':
					doPure = TRUE;
					break;

				case 'S':
					doCheckOnly = TRUE;
#ifdef __WIN32__
					/* Under Windows we can't fclose( stdout ) because the
					   VC++ runtime reassigns the stdout handle to the next
					   open file (which is valid) but then scribbles stdout
					   garbage all over it for files larger than about 16K
					   (which isn't), so we have to make sure that the
					   stdout is handle pointed to something somewhere */
					freopen( "nul", "w", stdout );
#else
					/* If we know we're running under Unix we can also
					   freopen( "/dev/null", "w", stdout ); */
					fclose( stdout );
#endif /* __WIN32__ */
					break;

				case 'T':
					dumpText = TRUE;
					break;

				case 'X':
					doHexValues = TRUE;
					break;

				case 'Z':
					zeroLengthAllowed = TRUE;
					break;

				default:
					printf( "Unknown argument '%c'.\n", *argPtr );
					return( EXIT_SUCCESS );
				}
			argPtr++;
			}
		argv++;
		argc--;
		}

	/* We can't use options which perform an fseek() if reading from stdin */
	if( useStdin && ( doDumpHeader || outFile != NULL ) )
		{
		puts( "Can't use -f or -h when taking input from stdin" );
		exit( EXIT_FAILURE );
		}

	/* Check args and read the config file.  We don't bother weeding out
	   dups during the read because (a) the linear search would make the
	   process n^2, (b) during the dump process the search will terminate on
	   the first match so dups aren't that serious, and (c) there should be
	   very few dups present */
	if( argc != 1 && !useStdin )
		usageExit();
	if( !readGlobalConfig( pathPtr ) )
		exit( EXIT_FAILURE );

	/* Dump the given file */
	if( useStdin )
		inFile = stdin;
	else
		if( ( inFile = fopen( argv[ 0 ], "rb" ) ) == NULL )
			{
			perror( argv[ 0 ] );
			exit( EXIT_FAILURE );
			}
	if( useStdin )
		{
		while( offset-- )
			getc( inFile );
		}
	else
		fseek( inFile, offset, SEEK_SET );
	if( outFile != NULL )
		{
		ASN1_ITEM item;
		long length;
		int i, status;

		/* Make sure there's something there, and that it has a definite
		   length */
		status = getItem( inFile, &item );
		if( status == -1 )
			{
			puts( "Non-ASN.1 data encountered." );
			exit( EXIT_FAILURE );
			}
		if( status == 0 )
			{
			puts( "Nothing to read." );
			exit( EXIT_FAILURE );
			}
		if( item.indefinite )
			{
			puts( "Cannot process indefinite-length item." );
			exit( EXIT_FAILURE );
			}

		/* Copy the item across, first the header and then the data */
		for( i = 0; i < item.headerSize; i++ )
			putc( item.header[ i ], outFile );
		for( length = 0; length < item.length && !feof( inFile ); length++ )
			putc( getc( inFile ), outFile );
		fclose( outFile );

		fseek( inFile, offset, SEEK_SET );
		}
	printAsn1( inFile, 0, LENGTH_MAGIC, 0 );
	fclose( inFile );

	/* Print a summary of warnings/errors if it's required or appropriate */
	if( !doPure )
		{
		if( !doCheckOnly )
			fputc( '\n', stderr );
		fprintf( stderr, "%d warning%s, %d error%s.\n", noWarnings,
				( noWarnings != 1 ) ? "s" : "", noErrors,
				( noErrors != 1 ) ? "s" : "" );
		}

	return( ( noErrors ) ? noErrors : EXIT_SUCCESS );
	}
