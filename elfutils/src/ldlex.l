%{
/* Copyright (C) 2001, 2002 Red Hat, Inc.
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

#define yylval ldlval
#define YY_DECL int ldlex (YYSTYPE *lval, struct ld_state *statep)

#include <assert.h>
#include <ctype.h>
#include <elf.h>
#include <error.h>
#include <inttypes.h>
#include <libintl.h>
#include <stdbool.h>
#include <string.h>

#include <system.h>
#include <ld.h>
#include "ldscript.h"

/* Define in ld.c.  */
extern int ld_scan_version_script;

#define MAX_PREPDEPTH 20
static enum prepstate
{
  prep_normal,
  skip_if,
  skip_to_endif
} prepstate[MAX_PREPDEPTH];
static int prepdepth;

static void eat_comment (void);
static void eat_to_eol (bool empty);
static int attrib_convert (int c);
static void push_state (enum prepstate);
static int pop_state (void);
static int handle_ifdef (struct ld_state *statep);
%}

ID		[a-zA-Z0-9_.*?]+
FILENAMECHAR1	[a-zA-Z0-9_/.\\~]
FILENAMECHAR	[^][{}[:space:]():;]+
HEX		0[xX][0-9a-fA-F]+[kKmM]?
OCT		0[0-7]*[kKmM]?
DEC		[0-9]+[kKmM]?
WHITE		[[:space:]]+

%option yylineno
%option never-interactive
%option noyywrap

%x IGNORE

%%
				if (__builtin_expect (ld_scan_version_script,
						      0) > 0)
				  {
				    ld_scan_version_script = -1;
				    return kVERSION_SCRIPT;
				  }

^"#"ifdef/[[:space:]]		{ BEGIN (handle_ifdef (statep)); }
^"#"else/[[:space:]\n]		{ eat_to_eol (true);
				  push_state (skip_to_endif);
				  BEGIN (IGNORE); }
^"#"elifdef/[[:space:]]		{ eat_to_eol (false);
				  push_state (skip_to_endif);
				  BEGIN (IGNORE); }
^"#"endif/[[:space:]\n]		{ eat_to_eol (true) ; }

<IGNORE>^"#"ifdef/[[:space:]\n] { eat_to_eol (false);
				  push_state (skip_to_endif); }
<IGNORE>^"#"else/[[:space:]\n]	{ eat_to_eol (true);
				  assert (prepdepth > 0);
				  if (prepstate[prepdepth - 1] == skip_if)
				    {
				      /* Back to normal processing.  */
				      assert (prepdepth == 1);
				      BEGIN (pop_state ());
				    }
				}
<IGNORE>^"#"elifdef/[[:space:]]	{ assert (prepdepth > 0);
				  if (prepstate[prepdepth - 1] == skip_if)
				    {
				      /* Maybe this symbol is defined.  */
				      pop_state ();
				      BEGIN (handle_ifdef (statep));
				    }
				}
<IGNORE>^"#"endif/[[:space:]\n] { eat_to_eol (true);
				  BEGIN (pop_state ()); }
<IGNORE>.|\n			{ /* nothing */ }


"/*"				{ eat_comment (); }

ALIGN				{ return kALIGN; }
ENTRY				{ return kENTRY; }
EXCLUDE_FILE			{ return kEXCLUDE_FILE; }
"global:"			{ return kGLOBAL; }
GROUP				{ return kGROUP; }
INPUT				{ return kINPUT; }
INTERP				{ return kINTERP; }
KEEP				{ return kKEEP; }
"local:"			{ return kLOCAL; }
PAGESIZE			{ return kPAGESIZE; }
PROVIDE				{ return kPROVIDE; }
SEARCH_DIR			{ return kSEARCH_DIR; }
SEGMENT				{ return kSEGMENT; }
SIZEOF_HEADERS			{ return kSIZEOF_HEADERS; }
SORT				{ return kSORT; }
VERSION				{ return kVERSION; }

"["([RWX]){0,3}"]"		{ int cnt = 1 ;
				  lval->num = 0;
				  while (cnt < yyleng - 1)
				    lval->num |= attrib_convert (yytext[cnt++]);
				  return kMODE; }

"{"				{ return '{'; }
"}"				{ return '}'; }
"("				{ return '('; }
")"				{ return ')'; }
":"				{ return ':'; }
";"				{ return ';'; }
"="				{ return '='; }
"+"				{ lval->op = exp_plus; return kADD_OP; }
"-"				{ lval->op = exp_minus; return kADD_OP; }
"*"				{ return '*'; }
"/"				{ lval->op = exp_div; return kMUL_OP; }
"%"				{ lval->op = exp_mod; return kMUL_OP; }
"&"				{ return '&'; }
"|"				{ return '|'; }

","				{ return ','; }

{HEX}|{OCT}|{DEC}		{ char *endp;
				  lval->num = strtoumax (yytext, &endp, 0);
				  if (*endp != '\0')
				    {
				      if (tolower (*endp) == 'k')
					lval->num *= 1024;
				      else
					{
					  assert (tolower (*endp) == 'm');
					  lval->num *= 1024 * 1024;
					}
				    }
				  return kNUM; }

{ID}				{ lval->str = xstrndup (yytext, yyleng);
				  return kID; }

{FILENAMECHAR1}{FILENAMECHAR}	{ lval->str = xstrndup (yytext, yyleng);
				  return kFILENAME; }

{WHITE}				{ /* IGNORE */ }

%%

static void
eat_comment (void)
{
  while (1)
    {
      int c = input ();

      while (c != '*' && c != EOF)
	c = input ();

      if (c == '*')
	{
	  c = input ();
	  while (c == '*')
	    c = input ();
	  if (c == '/')
	    break;
	}

      if (c == EOF)
	{
	  /* XXX Use the setjmp buffer and signal EOF in comment */
	  error (0, 0, gettext ("EOF in comment"));
	  break;
	}
    }
}


static void
eat_to_eol (bool empty)
{
  bool warned = false;

  while (1)
    {
      int c = input ();

      if (c == EOF)
	break;
      if (c == '\n')
	{
	  ++yylineno;
	  break;
	}

      if (empty && ! isspace (c) && ! warned)
	{
	  error (0, 0, gettext ("%d: garbage at end of line"), yylineno);
	  warned = true;
	}
    }
}


static int
attrib_convert (int c)
{
  if (c == 'X')
    return PF_X;
  if (c == 'W')
    return PF_W;
  assert (c == 'R');
  return PF_R;
}


static void
push_state (enum prepstate state)
{
  if (prepdepth >= MAX_PREPDEPTH)
    error (EXIT_FAILURE, 0, gettext ("%d: conditionals nested too deep"),
	   yylineno);

  prepstate[prepdepth++] = state;
}


static int
pop_state (void)
{
  if (prepdepth == 0)
    error (0, 0, gettext ("%d: unexpected #endif"), yylineno);
  else
    --prepdepth;

  return prepdepth == 0 ? INITIAL : IGNORE;
}


static int
handle_ifdef (struct ld_state *statep)
{
  char idbuf[50];
  char *id = idbuf;
  size_t idlen = 0;
  size_t idmax = sizeof (idbuf);
  bool ignore_ws = true;
  bool defined = false;
  int result;

  while (1)
    {
      int c = input ();

      if (isspace (c) && ignore_ws)
	continue;

      if (c != '_' && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z')
	  && (idlen == 0 || c < '0' || c > '9'))
	{
	  unput (c);
	  break;
	}

      if (idlen == idmax)
	{
	  char *newp = (char *) alloca (idmax *= 2);
	  id = memcpy (newp, id, idlen);
	}

      id[idlen++] = c;
      ignore_ws = false;
    }

  /* XXX Compare in a better way.  */
  if (idlen == 9 && strncmp (id, "COMBRELOC", 9) == 0)
    defined = statep->combreloc && statep->file_type != relocatable_file_type;
  else if (idlen == 6 && strncmp (id, "SHARED", 6) == 0)
    defined = statep->file_type == dso_file_type;

  if (defined)
    result = INITIAL;
  else
    {
      push_state (skip_if);
      result = IGNORE;
    }

  return result;
}


// Local Variables:
// mode: C
// End:
