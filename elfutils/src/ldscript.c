/* A Bison parser, made from ../../src/ldscript.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse ldparse
#define yylex ldlex
#define yyerror lderror
#define yylval ldlval
#define yychar ldchar
#define yydebug lddebug
#define yynerrs ldnerrs
# define	kADD_OP	257
# define	kALIGN	258
# define	kENTRY	259
# define	kEXCLUDE_FILE	260
# define	kFILENAME	261
# define	kGLOBAL	262
# define	kGROUP	263
# define	kID	264
# define	kINPUT	265
# define	kINTERP	266
# define	kKEEP	267
# define	kLOCAL	268
# define	kMODE	269
# define	kMUL_OP	270
# define	kNUM	271
# define	kPAGESIZE	272
# define	kPROVIDE	273
# define	kSEARCH_DIR	274
# define	kSEGMENT	275
# define	kSIZEOF_HEADERS	276
# define	kSORT	277
# define	kVERSION	278
# define	kVERSION_SCRIPT	279
# define	ADD_OP	280
# define	MUL_OP	281

#line 1 "../../src/ldscript.y"

/* Parser for linker scripts.
   Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <error.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <system.h>
#include <ld.h>

#define YYPARSE_PARAM	statep
#define YYLEX_PARAM	statep
#define YYPURE	1

/* The error handler.  */
static void yyerror (const char *s);

/* Some helper functions we need to construct the data structures
   describing information from the file.  */
static struct expression *new_expr (int tag);
static struct input_section_name *new_input_section_name (const char *name,
							  bool sort_flag);
static struct input_rule *new_input_rule (int tag);
static struct output_rule *new_output_rule (int tag);
static struct assignment *new_assignment (const char *variable,
					  struct expression *expression,
					  bool provide_flag);
static void new_segment (int mode, struct output_rule *output_rule,
			 struct ld_state *statep);
static struct filename_list *new_filename_listelem (const char *string);
static void add_inputfiles (struct filename_list *fnames,
			    struct ld_state *statep);
static struct id_list *new_id_listelem (const char *str);
static struct version *new_version (struct id_list *local,
				    struct id_list *global);
static struct version *merge_versions (struct version *one,
				       struct version *two);
static void add_versions (struct version *versions, struct ld_state *statep);

/* The lexer.  The prototype is a bit of a problem.  The first parameter
   should be 'YYSTYPE *' but YYSTYPE is not defined.  */
extern int yylex (void *, struct ld_state *);

#line 70 "../../src/ldscript.y"
#ifndef YYSTYPE
typedef union {
  uintmax_t num;
  enum expression_tag op;
  char *str;
  struct expression *expr;
  struct input_section_name *sectionname;
  struct filemask_section_name *filemask_section_name;
  struct input_rule *input_rule;
  struct output_rule *output_rule;
  struct assignment *assignment;
  struct filename_list *filename_list;
  struct version *version;
  struct id_list *id_list;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		142
#define	YYFLAG		-32768
#define	YYNTBASE	38

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 281 ? yytranslate[x] : 59)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    27,     2,
      31,    32,    30,     2,    37,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    33,
       2,    36,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    34,    26,    35,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      28,    29
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     5,     8,    10,    16,    22,    28,    34,
      40,    46,    51,    56,    61,    64,    66,    69,    74,    77,
      81,    88,    91,    93,    95,   100,   103,   109,   111,   116,
     121,   122,   127,   131,   135,   139,   143,   147,   151,   153,
     155,   157,   159,   163,   165,   167,   168,   171,   173,   178,
     184,   191,   194,   196,   199,   202,   206,   209,   211,   213,
     215
};
static const short yyrhs[] =
{
      39,     0,    25,    52,     0,    39,    40,     0,    40,     0,
       5,    31,    10,    32,    33,     0,    20,    31,    57,    32,
      33,     0,    18,    31,    17,    32,    33,     0,    12,    31,
      57,    32,    33,     0,    21,    15,    34,    41,    35,     0,
      21,     1,    34,    41,    35,     0,     9,    31,    50,    32,
       0,    11,    31,    50,    32,     0,    24,    34,    52,    35,
       0,    41,    42,     0,    42,     0,    43,    33,     0,    10,
      34,    44,    35,     0,    10,    33,     0,    10,    36,    49,
       0,    19,    31,    10,    36,    49,    32,     0,    44,    45,
       0,    45,     0,    46,     0,    13,    31,    46,    32,     0,
      43,    33,     0,    58,    31,    48,    47,    32,     0,    10,
       0,    23,    31,    10,    32,     0,     6,    31,    57,    32,
       0,     0,     4,    31,    49,    32,     0,    31,    49,    32,
       0,    49,    30,    49,     0,    49,    16,    49,     0,    49,
       3,    49,     0,    49,    27,    49,     0,    49,    26,    49,
       0,    17,     0,    10,     0,    22,     0,    18,     0,    50,
      51,    57,     0,    57,     0,    37,     0,     0,    52,    53,
       0,    53,     0,    34,    54,    35,    33,     0,    57,    34,
      54,    35,    33,     0,    57,    34,    54,    35,    57,    33,
       0,    54,    55,     0,    55,     0,     8,    56,     0,    14,
      56,     0,    56,    58,    33,     0,    58,    33,     0,     7,
       0,    10,     0,    57,     0,    30,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   136,   138,   142,   143,   146,   153,   157,   162,   169,
     173,   178,   189,   191,   195,   200,   204,   209,   221,   244,
     246,   250,   255,   259,   264,   271,   278,   289,   291,   295,
     297,   301,   306,   308,   314,   320,   326,   332,   338,   343,
     348,   350,   354,   360,   364,   365,   368,   373,   377,   383,
     389,   397,   400,   404,   406,   410,   417,   421,   423,   427,
     429
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "kADD_OP", "kALIGN", "kENTRY", 
  "kEXCLUDE_FILE", "kFILENAME", "kGLOBAL", "kGROUP", "kID", "kINPUT", 
  "kINTERP", "kKEEP", "kLOCAL", "kMODE", "kMUL_OP", "kNUM", "kPAGESIZE", 
  "kPROVIDE", "kSEARCH_DIR", "kSEGMENT", "kSIZEOF_HEADERS", "kSORT", 
  "kVERSION", "kVERSION_SCRIPT", "'|'", "'&'", "ADD_OP", "MUL_OP", "'*'", 
  "'('", "')'", "';'", "'{'", "'}'", "'='", "','", "script_or_version", 
  "file", "content", "outputsections", "outputsection", "assignment", 
  "inputsections", "inputsection", "sectionname", "sort_opt_name", 
  "exclude_opt", "expr", "filename_id_list", "comma_opt", "versionlist", 
  "version", "version_stmt_list", "version_stmt", "filename_id_star_list", 
  "filename_id", "filename_id_star", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    38,    38,    39,    39,    40,    40,    40,    40,    40,
      40,    40,    40,    40,    41,    41,    42,    42,    42,    43,
      43,    44,    44,    45,    45,    45,    46,    47,    47,    48,
      48,    49,    49,    49,    49,    49,    49,    49,    49,    49,
      49,    49,    50,    50,    51,    51,    52,    52,    53,    53,
      53,    54,    54,    55,    55,    56,    56,    57,    57,    58,
      58
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     2,     2,     1,     5,     5,     5,     5,     5,
       5,     4,     4,     4,     2,     1,     2,     4,     2,     3,
       6,     2,     1,     1,     4,     2,     5,     1,     4,     4,
       0,     4,     3,     3,     3,     3,     3,     3,     1,     1,
       1,     1,     3,     1,     1,     0,     2,     1,     4,     5,
       6,     2,     1,     2,     2,     3,     2,     1,     1,     1,
       1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       1,     4,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    57,    58,     0,     2,    47,     0,     3,     0,    45,
      43,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    52,    46,     0,     0,    11,    44,     0,    12,     0,
       0,     0,     0,     0,     0,    15,     0,     0,    13,    60,
      53,    59,     0,    54,     0,    51,     0,     5,    42,     8,
       7,     6,    18,     0,     0,     0,    10,    14,    16,     9,
       0,    56,    48,     0,    58,     0,     0,     0,    22,    23,
       0,     0,    39,    38,    41,    40,     0,    19,     0,    55,
      49,     0,     0,    25,    17,    21,    30,     0,     0,     0,
       0,     0,     0,     0,     0,    50,     0,     0,     0,     0,
      32,    35,    34,    37,    36,    33,     0,    24,     0,    27,
       0,     0,    31,    20,     0,     0,    26,    29,     0,    28,
       0,     0,     0
};

static const short yydefgoto[] =
{
     140,    10,    11,    54,    55,    56,    87,    88,    89,   131,
     118,    97,    29,    47,    24,    25,    40,    41,    60,    61,
      90
};

static const short yypact[] =
{
     112,   -29,   -14,   -12,    -6,    30,    58,    54,    80,     2,
     129,-32768,    89,    95,    95,    95,    73,    95,    82,    84,
       2,-32768,-32768,    65,     2,-32768,    93,-32768,    97,    74,
  -32768,    76,    99,   103,   107,     3,     3,    14,    85,    85,
      21,-32768,-32768,    65,   111,-32768,-32768,    95,-32768,   113,
     115,   118,    92,    91,    75,-32768,   119,    77,-32768,-32768,
      85,-32768,   122,    85,   130,-32768,    46,-32768,-32768,-32768,
  -32768,-32768,-32768,    90,    40,   152,-32768,-32768,-32768,-32768,
     131,-32768,-32768,    60,   132,   134,   133,    33,-32768,-32768,
     136,   138,-32768,-32768,-32768,-32768,    40,    50,   135,-32768,
  -32768,   137,    85,-32768,-32768,-32768,   166,    40,     0,    40,
      40,    40,    40,    40,    40,-32768,   141,   143,    -3,     7,
  -32768,    50,    50,    48,    56,    -2,    15,-32768,    95,-32768,
     144,   145,-32768,-32768,   146,   169,-32768,-32768,   148,-32768,
     176,   181,-32768
};

static const short yypgoto[] =
{
  -32768,-32768,   172,   147,    88,    11,-32768,    98,    86,-32768,
  -32768,    47,   170,-32768,   167,    64,   149,    25,   150,    -9,
      44
};


#define	YYLAST		192


static const short yytable[] =
{
      26,   109,    12,   109,    30,    30,    32,   129,    34,    21,
     109,    26,    22,    52,   110,    26,   110,    13,   109,    14,
     130,    21,    53,   110,    22,    15,   111,   112,    26,    38,
     113,   110,   120,   111,   112,    39,    23,   113,    68,   132,
      21,   111,   112,    84,    91,   113,    85,   133,    23,    58,
      92,   109,    53,   109,    38,    18,    64,    93,    94,   109,
      39,    16,    95,    59,   110,    65,   110,    21,   104,    19,
      22,    96,   110,    38,   101,   112,   111,   112,   113,    39,
     113,    83,    62,    62,    86,    52,   113,    52,    42,    17,
      33,    65,    21,   100,    53,    22,    53,    21,    86,    28,
      84,    42,    21,    85,    80,    22,    45,    80,    48,    53,
      76,    46,    79,    46,    20,    59,    35,     1,    36,   134,
      59,     2,    75,     3,     4,    72,    73,    43,    74,    44,
       5,    49,     6,     7,     1,    50,     8,     9,     2,    51,
       3,     4,    77,   108,    67,    77,    69,     5,    70,     6,
       7,    71,    78,     8,   119,    81,   121,   122,   123,   124,
     125,   126,    98,    82,    99,   102,   103,   106,    74,   107,
     115,   114,   117,   127,   128,   135,   141,   136,   137,   138,
     139,   142,    27,    57,    31,   105,     0,    37,   116,    63,
       0,     0,    66
};

static const short yycheck[] =
{
       9,     3,    31,     3,    13,    14,    15,    10,    17,     7,
       3,    20,    10,    10,    16,    24,    16,    31,     3,    31,
      23,     7,    19,    16,    10,    31,    26,    27,    37,     8,
      30,    16,    32,    26,    27,    14,    34,    30,    47,    32,
       7,    26,    27,    10,     4,    30,    13,    32,    34,    35,
      10,     3,    19,     3,     8,     1,    35,    17,    18,     3,
      14,    31,    22,    30,    16,    40,    16,     7,    35,    15,
      10,    31,    16,     8,    83,    27,    26,    27,    30,    14,
      30,    35,    38,    39,    73,    10,    30,    10,    24,    31,
      17,    66,     7,    33,    19,    10,    19,     7,    87,    10,
      10,    37,     7,    13,    60,    10,    32,    63,    32,    19,
      35,    37,    35,    37,    34,    30,    34,     5,    34,   128,
      30,     9,    31,    11,    12,    33,    34,    34,    36,    32,
      18,    32,    20,    21,     5,    32,    24,    25,     9,    32,
      11,    12,    54,    96,    33,    57,    33,    18,    33,    20,
      21,    33,    33,    24,   107,    33,   109,   110,   111,   112,
     113,   114,    10,    33,    33,    31,    33,    31,    36,    31,
      33,    36,     6,    32,    31,    31,     0,    32,    32,    10,
      32,     0,    10,    36,    14,    87,    -1,    20,   102,    39,
      -1,    -1,    43
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 2:
#line 139 "../../src/ldscript.y"
{ add_versions (yyvsp[0].version, statep); }
    break;
case 5:
#line 147 "../../src/ldscript.y"
{
		      if (((struct ld_state *) statep)->entry == NULL)
			((struct ld_state *) statep)->entry = yyvsp[-2].str;
		      else
			free (yyvsp[-2].str);
		    }
    break;
case 6:
#line 154 "../../src/ldscript.y"
{
		      ld_new_searchdir (yyvsp[-2].str, statep);
		    }
    break;
case 7:
#line 158 "../../src/ldscript.y"
{
		      if (((struct ld_state *) statep)->pagesize == 0)
			((struct ld_state *) statep)->pagesize = yyvsp[-2].num;
		    }
    break;
case 8:
#line 163 "../../src/ldscript.y"
{
		      if (((struct ld_state *) statep)->interp == NULL)
			((struct ld_state *) statep)->interp = yyvsp[-2].str;
		      else
			free (yyvsp[-2].str);
		    }
    break;
case 9:
#line 170 "../../src/ldscript.y"
{
		      new_segment (yyvsp[-3].num, yyvsp[-1].output_rule, statep);
		    }
    break;
case 10:
#line 174 "../../src/ldscript.y"
{
		      fputs (gettext ("mode for segment invalid\n"), stderr);
		      new_segment (0, yyvsp[-1].output_rule, statep);
		    }
    break;
case 11:
#line 179 "../../src/ldscript.y"
{
		      /* First little optimization.  If there is only one
			 file in the group don't do anything.  */
		      if (yyvsp[-1].filename_list != yyvsp[-1].filename_list->next)
			{
			  yyvsp[-1].filename_list->next->group_start = 1;
			  yyvsp[-1].filename_list->group_end = 1;
			}
		      add_inputfiles (yyvsp[-1].filename_list, statep);
		    }
    break;
case 12:
#line 190 "../../src/ldscript.y"
{ add_inputfiles (yyvsp[-1].filename_list, statep); }
    break;
case 13:
#line 192 "../../src/ldscript.y"
{ add_versions (yyvsp[-1].version, statep); }
    break;
case 14:
#line 196 "../../src/ldscript.y"
{
		      yyvsp[0].output_rule->next = yyvsp[-1].output_rule->next;
		      yyval.output_rule = yyvsp[-1].output_rule->next = yyvsp[0].output_rule;
		    }
    break;
case 15:
#line 201 "../../src/ldscript.y"
{ yyval.output_rule = yyvsp[0].output_rule; }
    break;
case 16:
#line 205 "../../src/ldscript.y"
{
		      yyval.output_rule = new_output_rule (output_assignment);
		      yyval.output_rule->val.assignment = yyvsp[-1].assignment;
		    }
    break;
case 17:
#line 210 "../../src/ldscript.y"
{
		      yyval.output_rule = new_output_rule (output_section);
		      yyval.output_rule->val.section.name = yyvsp[-3].str;
		      yyval.output_rule->val.section.input = yyvsp[-1].input_rule->next;
		      if (((struct ld_state *) statep)->strip == strip_debug
			  && IS_DEBUGSCN_P (yyvsp[-3].str, ((struct ld_state *) statep)))
			yyval.output_rule->val.section.ignored = true;
		      else
			yyval.output_rule->val.section.ignored = false;
		      yyvsp[-1].input_rule->next = NULL;
		    }
    break;
case 18:
#line 222 "../../src/ldscript.y"
{
		      /* This is a short cut for "ID { *(ID) }".  */
		      yyval.output_rule = new_output_rule (output_section);
		      yyval.output_rule->val.section.name = yyvsp[-1].str;
		      yyval.output_rule->val.section.input = new_input_rule (input_section);
		      yyval.output_rule->val.section.input->next = NULL;
		      yyval.output_rule->val.section.input->val.section =
			(struct filemask_section_name *)
			  xmalloc (sizeof (struct filemask_section_name));
		      yyval.output_rule->val.section.input->val.section->filemask = NULL;
		      yyval.output_rule->val.section.input->val.section->excludemask = NULL;
		      yyval.output_rule->val.section.input->val.section->section_name =
			new_input_section_name (yyvsp[-1].str, false);
		      yyval.output_rule->val.section.input->val.section->keep_flag = false;
		      if (((struct ld_state *) statep)->strip == strip_debug
			  && IS_DEBUGSCN_P (yyvsp[-1].str, ((struct ld_state *) statep)))
			yyval.output_rule->val.section.ignored = true;
		      else
			yyval.output_rule->val.section.ignored = false;
		    }
    break;
case 19:
#line 245 "../../src/ldscript.y"
{ yyval.assignment = new_assignment (yyvsp[-2].str, yyvsp[0].expr, false); }
    break;
case 20:
#line 247 "../../src/ldscript.y"
{ yyval.assignment = new_assignment (yyvsp[-3].str, yyvsp[-1].expr, true); }
    break;
case 21:
#line 251 "../../src/ldscript.y"
{
		      yyvsp[0].input_rule->next = yyvsp[-1].input_rule->next;
		      yyval.input_rule = yyvsp[-1].input_rule->next = yyvsp[0].input_rule;
		    }
    break;
case 22:
#line 256 "../../src/ldscript.y"
{ yyval.input_rule = yyvsp[0].input_rule; }
    break;
case 23:
#line 260 "../../src/ldscript.y"
{
		      yyval.input_rule = new_input_rule (input_section);
		      yyval.input_rule->val.section = yyvsp[0].filemask_section_name;
		    }
    break;
case 24:
#line 265 "../../src/ldscript.y"
{
		      yyvsp[-1].filemask_section_name->keep_flag = true;

		      yyval.input_rule = new_input_rule (input_section);
		      yyval.input_rule->val.section = yyvsp[-1].filemask_section_name;
		    }
    break;
case 25:
#line 272 "../../src/ldscript.y"
{
		      yyval.input_rule = new_input_rule (input_assignment);
		      yyval.input_rule->val.assignment = yyvsp[-1].assignment;
		    }
    break;
case 26:
#line 279 "../../src/ldscript.y"
{
		      yyval.filemask_section_name = (struct filemask_section_name *)
			xmalloc (sizeof (*yyval.filemask_section_name));
		      yyval.filemask_section_name->filemask = yyvsp[-4].str;
		      yyval.filemask_section_name->excludemask = yyvsp[-2].str;
		      yyval.filemask_section_name->section_name = yyvsp[-1].sectionname;
		      yyval.filemask_section_name->keep_flag = false;
		    }
    break;
case 27:
#line 290 "../../src/ldscript.y"
{ yyval.sectionname = new_input_section_name (yyvsp[0].str, false); }
    break;
case 28:
#line 292 "../../src/ldscript.y"
{ yyval.sectionname = new_input_section_name (yyvsp[-1].str, true); }
    break;
case 29:
#line 296 "../../src/ldscript.y"
{ yyval.str = yyvsp[-1].str; }
    break;
case 30:
#line 298 "../../src/ldscript.y"
{ yyval.str = NULL; }
    break;
case 31:
#line 302 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_align);
		      yyval.expr->val.child = yyvsp[-1].expr;
		    }
    break;
case 32:
#line 307 "../../src/ldscript.y"
{ yyval.expr = yyvsp[-1].expr; }
    break;
case 33:
#line 309 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_mult);
		      yyval.expr->val.binary.left = yyvsp[-2].expr;
		      yyval.expr->val.binary.right = yyvsp[0].expr;
		    }
    break;
case 34:
#line 315 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (yyvsp[-1].op);
		      yyval.expr->val.binary.left = yyvsp[-2].expr;
		      yyval.expr->val.binary.right = yyvsp[0].expr;
		    }
    break;
case 35:
#line 321 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (yyvsp[-1].op);
		      yyval.expr->val.binary.left = yyvsp[-2].expr;
		      yyval.expr->val.binary.right = yyvsp[0].expr;
		    }
    break;
case 36:
#line 327 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_and);
		      yyval.expr->val.binary.left = yyvsp[-2].expr;
		      yyval.expr->val.binary.right = yyvsp[0].expr;
		    }
    break;
case 37:
#line 333 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_or);
		      yyval.expr->val.binary.left = yyvsp[-2].expr;
		      yyval.expr->val.binary.right = yyvsp[0].expr;
		    }
    break;
case 38:
#line 339 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_num);
		      yyval.expr->val.num = yyvsp[0].num;
		    }
    break;
case 39:
#line 344 "../../src/ldscript.y"
{
		      yyval.expr = new_expr (exp_id);
		      yyval.expr->val.str = yyvsp[0].str;
		    }
    break;
case 40:
#line 349 "../../src/ldscript.y"
{ yyval.expr = new_expr (exp_sizeof_headers); }
    break;
case 41:
#line 351 "../../src/ldscript.y"
{ yyval.expr = new_expr (exp_pagesize); }
    break;
case 42:
#line 355 "../../src/ldscript.y"
{
		      struct filename_list *newp = new_filename_listelem (yyvsp[0].str);
		      newp->next = yyvsp[-2].filename_list->next;
		      yyval.filename_list = yyvsp[-2].filename_list->next = newp;
		    }
    break;
case 43:
#line 361 "../../src/ldscript.y"
{ yyval.filename_list = new_filename_listelem (yyvsp[0].str); }
    break;
case 46:
#line 369 "../../src/ldscript.y"
{
		      yyvsp[0].version->next = yyvsp[-1].version->next;
		      yyval.version = yyvsp[-1].version->next = yyvsp[0].version;
		    }
    break;
case 47:
#line 374 "../../src/ldscript.y"
{ yyval.version = yyvsp[0].version; }
    break;
case 48:
#line 378 "../../src/ldscript.y"
{
		      yyvsp[-2].version->versionname = "";
		      yyvsp[-2].version->parentname = NULL;
		      yyval.version = yyvsp[-2].version;
		    }
    break;
case 49:
#line 384 "../../src/ldscript.y"
{
		      yyvsp[-2].version->versionname = yyvsp[-4].str;
		      yyvsp[-2].version->parentname = NULL;
		      yyval.version = yyvsp[-2].version;
		    }
    break;
case 50:
#line 390 "../../src/ldscript.y"
{
		      yyvsp[-3].version->versionname = yyvsp[-5].str;
		      yyvsp[-3].version->parentname = yyvsp[-1].str;
		      yyval.version = yyvsp[-3].version;
		    }
    break;
case 51:
#line 399 "../../src/ldscript.y"
{ yyval.version = merge_versions (yyvsp[-1].version, yyvsp[0].version); }
    break;
case 52:
#line 401 "../../src/ldscript.y"
{ yyval.version = yyvsp[0].version; }
    break;
case 53:
#line 405 "../../src/ldscript.y"
{ yyval.version = new_version (NULL, yyvsp[0].id_list); }
    break;
case 54:
#line 407 "../../src/ldscript.y"
{ yyval.version = new_version (yyvsp[0].id_list, NULL); }
    break;
case 55:
#line 412 "../../src/ldscript.y"
{
		      struct id_list *newp = new_id_listelem (yyvsp[-1].str);
		      newp->next = yyvsp[-2].id_list->next;
		      yyval.id_list = yyvsp[-2].id_list->next = newp;
		    }
    break;
case 56:
#line 418 "../../src/ldscript.y"
{ yyval.id_list = new_id_listelem (yyvsp[-1].str); }
    break;
case 57:
#line 422 "../../src/ldscript.y"
{ yyval.str = yyvsp[0].str; }
    break;
case 58:
#line 424 "../../src/ldscript.y"
{ yyval.str = yyvsp[0].str; }
    break;
case 59:
#line 428 "../../src/ldscript.y"
{ yyval.str = yyvsp[0].str; }
    break;
case 60:
#line 430 "../../src/ldscript.y"
{ yyval.str = NULL; }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 433 "../../src/ldscript.y"


static void
yyerror (const char *s)
{
  extern int ldlineno;
  extern int ld_scan_version_script;

  if (ld_scan_version_script)
    fprintf (stderr, gettext ("while reading version script: %s at line %d\n"),
	     gettext (s), ldlineno);
  else
    fprintf (stderr, gettext ("while reading linker script: %s at line %d\n"),
	     gettext (s), ldlineno);
}


static struct expression *
new_expr (int tag)
{
  struct expression *newp = (struct expression *) xmalloc (sizeof (*newp));

  newp->tag = tag;
  return newp;
}


static struct input_section_name *
new_input_section_name (const char *name, bool sort_flag)
{
  struct input_section_name *newp =
    (struct input_section_name *) xmalloc (sizeof (*newp));

  newp->name = name;
  newp->sort_flag = sort_flag;
  return newp;
}


static struct input_rule *
new_input_rule (int tag)
{
  struct input_rule *newp = (struct input_rule *) xmalloc (sizeof (*newp));

  newp->tag = tag;
  newp->next = newp;
  return newp;
}


static struct output_rule *
new_output_rule (int tag)
{
  struct output_rule *newp = (struct output_rule *) xcalloc (1,
							     sizeof (*newp));

  newp->tag = tag;
  newp->next = newp;
  return newp;
}


static struct assignment *
new_assignment (const char *variable, struct expression *expression,
		bool provide_flag)
{
  struct assignment *newp = (struct assignment *) xmalloc (sizeof (*newp));

  newp->variable = variable;
  newp->expression = expression;
  newp->provide_flag = provide_flag;
  return newp;
}


static void
new_segment (int mode, struct output_rule *output_rule,
	     struct ld_state *statep)
{
  struct output_segment *newp;

  newp = (struct output_segment *) xmalloc (sizeof (*newp));
  newp->mode = mode;
  newp->next = newp;

  newp->output_rules = output_rule->next;
  output_rule->next = NULL;

  /* Enqueue the output segment description.  */
  if (statep->output_segments == NULL)
    statep->output_segments = newp;
  else
    {
      newp->next = statep->output_segments->next;
      statep->output_segments = statep->output_segments->next = newp;
    }

  /* If the output file should be stripped of all symbol set the flag
     in the structures of all output sections.  */
  if (mode == 0 && statep->strip == strip_all)
    {
      struct output_rule *runp;

      for (runp = newp->output_rules; runp != NULL; runp = runp->next)
	if (runp->tag == output_section)
	  runp->val.section.ignored = true;
    }
}


static struct filename_list *
new_filename_listelem (const char *string)
{
  struct filename_list *newp;

  newp = (struct filename_list *) xcalloc (1, sizeof (*newp));
  newp->name = string;
  newp->next = newp;
  return newp;
}


static void
add_inputfiles (struct filename_list *fnames, struct ld_state *statep)
{
  assert (fnames != NULL);

  if (statep->srcfiles == NULL)
    statep->srcfiles = fnames;
  else
    {
      struct filename_list *first = statep->srcfiles->next;

      statep->srcfiles->next = fnames->next;
      fnames->next = first;
      statep->srcfiles->next = fnames;
    }
}


static _Bool
special_char_p (const char *str)
{
  while (*str != '\0')
    {
      if (__builtin_expect (*str == '*', 0)
	  || __builtin_expect (*str == '?', 0)
	  || __builtin_expect (*str == '[', 0))
	return true;

      ++str;
    }

  return false;
}


static struct id_list *
new_id_listelem (const char *str)
{
  struct id_list *newp;

  newp = (struct id_list *) xmalloc (sizeof (*newp));
  if (str == NULL)
    newp->u.id_type = id_all;
  else if (__builtin_expect (special_char_p (str), false))
    newp->u.id_type = id_wild;
  else
    newp->u.id_type = id_str;
  newp->id = str;
  newp->next = newp;

  return newp;
}


static struct version *
new_version (struct id_list *local, struct id_list *global)
{
  struct version *newp;

  newp = (struct version *) xmalloc (sizeof (*newp));
  newp->next = newp;
  newp->local_names = local;
  newp->global_names = global;
  newp->versionname = NULL;
  newp->parentname = NULL;

  return newp;
}


static struct version *
merge_versions (struct version *one, struct version *two)
{
  assert (two->local_names == NULL || two->global_names == NULL);

  if (two->local_names != NULL)
    {
      if (one->local_names == NULL)
	one->local_names = two->local_names;
      else
	{
	  two->local_names->next = one->local_names->next;
	  one->local_names = one->local_names->next = two->local_names;
	}
    }
  else
    {
      if (one->global_names == NULL)
	one->global_names = two->global_names;
      else
	{
	  two->global_names->next = one->global_names->next;
	  one->global_names = one->global_names->next = two->global_names;
	}
    }

  free (two);

  return one;
}


static void
add_id_list (const char *versionname, struct id_list *runp, _Bool local,
	     struct ld_state *statep)
{
  struct id_list *lastp = runp;

  if (runp == NULL)
    /* Nothing to do.  */
    return;

  /* Convert into a simple single-linked list.  */
  runp = runp->next;
  assert (runp != NULL);
  lastp->next = NULL;

  do
    if (runp->u.id_type == id_str)
      {
	struct id_list *curp;
	struct id_list *defp;
	unsigned long int hval = elf_hash (runp->id);

	curp = runp;
	runp = runp->next;

	defp = ld_version_str_tab_find (&statep->version_str_tab, hval, curp);
	if (defp != NULL)
	  {
	    /* There is already a version definition for this symbol.  */
	    while (strcmp (defp->u.s.versionname, versionname) != 0)
	      {
		if (defp->next == NULL)
		  {
		    /* No version like this so far.  */
		    defp->next = curp;
		    curp->u.s.local = local;
		    curp->u.s.versionname = versionname;
		    curp->next = NULL;
		    defp = NULL;
		    break;
		  }

		defp = defp->next;
	      }

	    if (defp != NULL && defp->u.s.local != local)
	      error (EXIT_FAILURE, 0, versionname[0] == '\0'
		     ? gettext ("\
symbol '%s' in declared both local and global for unnamed version")
		     : gettext ("\
symbol '%s' in declared both local and global for version '%s'"),
		     runp->id, versionname);
	  }
	else
	  {
	    /* This is the first version definition for this symbol.  */
	    ld_version_str_tab_insert (&statep->version_str_tab, hval, curp);

	    curp->u.s.local = local;
	    curp->u.s.versionname = versionname;
	    curp->next = NULL;
	  }
      }
    else if (runp->u.id_type == id_all)
      {
	if (local)
	  {
	    if (statep->default_bind_global)
	      error (EXIT_FAILURE, 0,
		     gettext ("default visibility set as local and global"));
	    statep->default_bind_local = true;
	  }
	else
	  {
	    if (statep->default_bind_local)
	      error (EXIT_FAILURE, 0,
		     gettext ("default visibility set as local and global"));
	    statep->default_bind_global = true;
	  }

	runp = runp->next;
      }
    else
      {
	assert (runp->u.id_type == id_wild);
	/* XXX TBI */
	abort ();
      }
  while (runp != NULL);
}


static void
add_versions (struct version *versions, struct ld_state *statep)
{
  struct version *lastp = versions;

  if (versions == NULL)
    return;

  /* Convert into a simple single-linked list.  */
  versions = versions->next;
  assert (versions != NULL);
  lastp->next = NULL;

  do
    {
      struct version *oldp;

      add_id_list (versions->versionname, versions->local_names, true,
		   statep);
      add_id_list (versions->versionname, versions->global_names, false,
		   statep);

      oldp = versions;
      versions = versions->next;
      free (oldp);
    }
  while (versions != NULL);
}
