/*
 * Simple Expression Parser
 * Copyright (C) 1998 Tom Dyas <tdyas@eden.rutgers.edu>
 *
 * This module implements a basic expression parser with support for
 * integer and string datatypes. For ease of programming, we use the
 * top-down "recursive descent" method of parsing. While a
 * table-driven bottom-up parser might be faster, it does not really
 * matter for the expressions we will be parsing.
 *
 * This work is provided under the GPL or LGPL at your choice.
 */

#include "system.h"

#include "rpmbuild.h"
#include "rpmlib.h"

/* #define DEBUG_PARSER 1 */

#ifdef DEBUG_PARSER
#include <stdio.h>
#define DEBUG(x) do { x ; } while (0)
#else
#define DEBUG(x)
#endif


/*
 * Encapsulation of a "value"
 */

typedef struct _value
{
  enum { VALUE_TYPE_INTEGER, VALUE_TYPE_STRING } type;
  union {
    char *s;
    int i;
  } data;
} *Value;

static Value valueMakeInteger(int i)
{
  Value v;

  v = (Value) xmalloc(sizeof(struct _value));
  v->type = VALUE_TYPE_INTEGER;
  v->data.i = i;
  return v;
}

static Value valueMakeString(const char *s)
{
  Value v;

  v = (Value) xmalloc(sizeof(struct _value));
  v->type = VALUE_TYPE_STRING;
  v->data.s = xstrdup(s);
  return v;
}

static void valueFree( /*@only@*/ Value v)
{
  if (v) {
    if (v->type == VALUE_TYPE_STRING) free(v->data.s);
    free(v);
  }
}

#ifdef DEBUG_PARSER
static void valueDump(const char *msg, Value v, FILE *fp)
{
  if (msg)
    fprintf(fp, "%s ", msg);
  if (v) {
    if (v->type == VALUE_TYPE_INTEGER)
      fprintf(fp, "INTEGER %d\n", v->data.i);
    else
      fprintf(fp, "STRING '%s'\n", v->data.s);
  } else
    fprintf(fp, "NULL\n");
}
#endif

#define valueIsInteger(v) ((v)->type == VALUE_TYPE_INTEGER)
#define valueIsString(v) ((v)->type == VALUE_TYPE_STRING)
#define valueSameType(v1,v2) ((v1)->type == (v2)->type)


/*
 * Parser state.
 */

typedef struct _parseState {
  /*@owned@*/ char *str;	/* expression string */
  /*@dependent@*/ char *p;	/* current position in expression string */
  int nextToken;		/* current lookahead token */
  Value tokenValue;		/* valid when TOK_INTEGER or TOK_STRING */
  Spec spec;			/* spec file that we are parsing inside of */
} *ParseState;


/*
 * Token parser.
 */

#define TOK_EOF          1
#define TOK_INTEGER      2
#define TOK_STRING       3
#define TOK_IDENTIFIER   4
#define TOK_ADD          5
#define TOK_MINUS        6
#define TOK_MULTIPLY     7
#define TOK_DIVIDE       8
#define TOK_OPEN_P       9
#define TOK_CLOSE_P     10
#define TOK_EQ          11
#define TOK_NEQ         12
#define TOK_LT          13
#define TOK_LE          14
#define TOK_GT          15
#define TOK_GE          16
#define TOK_NOT         17
#define TOK_LOGICAL_AND 18
#define TOK_LOGICAL_OR  19

#define	EXPRBUFSIZ	BUFSIZ

#if defined(DEBUG_PARSER)
typedef struct exprTokTableEntry {
    const char *name;
    int val;
} ETTE_t;

ETTE_t exprTokTable[] = {
    { "EOF",	TOK_EOF },
    { "I",	TOK_INTEGER },
    { "S",	TOK_STRING },
    { "ID",	TOK_IDENTIFIER },
    { "+",	TOK_ADD },
    { "-",	TOK_MINUS },
    { "*",	TOK_MULTIPLY },
    { "/",	TOK_DIVIDE },
    { "( ",	TOK_OPEN_P },
    { " )",	TOK_CLOSE_P },
    { "==",	TOK_EQ },
    { "!=",	TOK_NEQ },
    { "<",	TOK_LT },
    { "<=",	TOK_LE },
    { ">",	TOK_GT },
    { ">=",	TOK_GE },
    { "!",	TOK_NOT },
    { "&&",	TOK_LOGICAL_AND },
    { "||",	TOK_LOGICAL_OR },
    { NULL, 0 }
};

static const char *prToken(int val)
{
    ETTE_t *et;
    
    for (et = exprTokTable; et->name != NULL; et++) {
	if (val == et->val)
	    return et->name;
    }
    return "???";
}
#endif	/* DEBUG_PARSER */

static int rdToken(ParseState state)
{
  int token;
  Value v = NULL;
  char *p = state->p;

  /* Skip whitespace before the next token. */
  while (*p && isspace(*p)) p++;

  switch (*p) {
  case '\0':
    token = TOK_EOF;
    p--;
    break;
  case '+':
    token = TOK_ADD;
    break;
  case '-':
    token = TOK_MINUS;
    break;
  case '*':
    token = TOK_MULTIPLY;
    break;
  case '/':
    token = TOK_DIVIDE;
    break;
  case '(':
    token = TOK_OPEN_P;
    break;
  case ')':
    token = TOK_CLOSE_P;
    break;
  case '=':
    if (p[1] == '=') {
      token = TOK_EQ;
      p++;
    } else {
      rpmError(RPMERR_BADSPEC, _("syntax error while parsing =="));
      return -1;
    }
    break;
  case '!':
    if (p[1] == '=') {
      token = TOK_NEQ;
      p++;
    } else
      token = TOK_NOT;
    break;
  case '<':
    if (p[1] == '=') {
      token = TOK_LE;
      p++;
    } else
      token = TOK_LT;
    break;
  case '>':
    if (p[1] == '=') {
      token = TOK_GE;
      p++;
    } else
      token = TOK_GT;
    break;
  case '&':
    if (p[1] == '&') {
      token = TOK_LOGICAL_AND;
      p++;
    } else {
      rpmError(RPMERR_BADSPEC, _("syntax error while parsing &&"));
      return -1;
    }
    break;
  case '|':
    if (p[1] == '|') {
      token = TOK_LOGICAL_OR;
      p++;
    } else {
      rpmError(RPMERR_BADSPEC, _("syntax error while parsing ||"));
      return -1;
    }
    break;

  default:
    if (isdigit(*p)) {
      char temp[EXPRBUFSIZ], *t = temp;

      while (*p && isdigit(*p))
	*t++ = *p++;
      *t++ = '\0';
      p--;

      token = TOK_INTEGER;
      v = valueMakeInteger(atoi(temp));

    } else if (isalpha(*p)) {
      char temp[EXPRBUFSIZ], *t = temp;

      while (*p && (isalnum(*p) || *p == '_'))
	*t++ = *p++;
      *t++ = '\0';
      p--;

      token = TOK_IDENTIFIER;
      v = valueMakeString(temp);

    } else if (*p == '\"') {
      char temp[EXPRBUFSIZ], *t = temp;

      p++;
      while (*p && *p != '\"')
	*t++ = *p++;
      *t++ = '\0';

      expandMacros(state->spec, state->spec->macros, temp, sizeof(temp));

      token = TOK_STRING;
      v = valueMakeString(temp);

    } else {
      rpmError(RPMERR_BADSPEC, _("parse error in expression"));
      return -1;
    }
  }

  state->p = p + 1;
  state->nextToken = token;
  state->tokenValue = v;

  DEBUG(printf("rdToken: \"%s\" (%d)\n", prToken(token), token));
  DEBUG(valueDump("rdToken:", state->tokenValue, stdout));

  return 0;
}


static Value doLogical(ParseState state);

static Value doPrimary(ParseState state)
{
  Value v;

  DEBUG(printf("doPrimary()\n"));

  switch (state->nextToken) {
  case TOK_OPEN_P:
    if (rdToken(state))
      return NULL;
    v = doLogical(state);
    if (state->nextToken != TOK_CLOSE_P) {
      rpmError(RPMERR_BADSPEC, _("unmatched ("));
      return NULL;
    }
    break;

  case TOK_INTEGER:
  case TOK_STRING:
    v = state->tokenValue;
    if (rdToken(state))
      return NULL;
    break;

  case TOK_IDENTIFIER: {
    char *name = state->tokenValue->data.s;
    const char *body;

    body = getMacroBody(state->spec->macros, name);
    if (!body) {
      rpmError(RPMERR_BADSPEC, _("undefined identifier"));
      return NULL;
    }

    v = valueMakeString(body);
    if (rdToken(state))
      return NULL;
    break;
  }

  case TOK_MINUS:
    if (rdToken(state))
      return NULL;

    v = doPrimary(state);
    if (v == NULL)
      return NULL;

    if (! valueIsInteger(v)) {
      rpmError(RPMERR_BADSPEC, _("- only on numbers"));
      return NULL;
    }

    v = valueMakeInteger(- v->data.i);
    break;

  case TOK_NOT:
    if (rdToken(state))
      return NULL;

    v = doPrimary(state);
    if (v == NULL)
      return NULL;

    if (! valueIsInteger(v)) {
      rpmError(RPMERR_BADSPEC, _("! only on numbers"));
      return NULL;
    }

    v = valueMakeInteger(! v->data.i);
    break;
  default:
    return NULL;
    /*@notreached@*/ break;
  }

  DEBUG(valueDump("doPrimary:", v, stdout));
  return v;
}

static Value doMultiplyDivide(ParseState state)
{
  Value v1, v2 = NULL;

  DEBUG(printf("doMultiplyDivide()\n"));

  v1 = doPrimary(state);
  if (v1 == NULL)
    return NULL;

  while (state->nextToken == TOK_MULTIPLY
	 || state->nextToken == TOK_DIVIDE) {
    int op = state->nextToken;

    if (rdToken(state))
      return NULL;

    if (v2) valueFree(v2);

    v2 = doPrimary(state);
    if (v2 == NULL)
      return NULL;

    if (! valueSameType(v1, v2)) {
      rpmError(RPMERR_BADSPEC, _("types must match"));
      return NULL;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      valueFree(v1);
      if (op == TOK_MULTIPLY)
	v1 = valueMakeInteger(i1 * i2);
      else
	v1 = valueMakeInteger(i1 / i2);
    } else {
      rpmError(RPMERR_BADSPEC, _("* / not suported for strings"));
      return NULL;
    }
  }

  if (v2) valueFree(v2);
  return v1;
}

static Value doAddSubtract(ParseState state)
{
  Value v1, v2 = NULL;

  DEBUG(printf("doAddSubtract()\n"));

  v1 = doMultiplyDivide(state);
  if (v1 == NULL)
    return NULL;

  while (state->nextToken == TOK_ADD || state->nextToken == TOK_MINUS) {
    int op = state->nextToken;

    if (rdToken(state))
      return NULL;

    if (v2) valueFree(v2);

    v2 = doMultiplyDivide(state);
    if (v2 == NULL)
      return NULL;

    if (! valueSameType(v1, v2)) {
      rpmError(RPMERR_BADSPEC, _("types must match"));
      return NULL;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      valueFree(v1);
      if (op == TOK_ADD)
	v1 = valueMakeInteger(i1 + i2);
      else
	v1 = valueMakeInteger(i1 - i2);
    } else {
      char *copy;

      if (op == TOK_MINUS) {
	rpmError(RPMERR_BADSPEC, _("- not suported for strings"));
	return NULL;
      }

      copy = xmalloc(strlen(v1->data.s) + strlen(v2->data.s) + 1);
      strcpy(copy, v1->data.s);
      strcat(copy, v2->data.s);

      valueFree(v1);
      v1 = valueMakeString(copy);
      free(copy);
    }
  }

  if (v2) valueFree(v2);
  return v1;
}

static Value doRelational(ParseState state)
{
  Value v1, v2 = NULL;

  DEBUG(printf("doRelational()\n"));

  v1 = doAddSubtract(state);
  if (v1 == NULL)
    return NULL;

  while (state->nextToken >= TOK_EQ && state->nextToken <= TOK_GE) {
    int op = state->nextToken;

    if (rdToken(state))
      return NULL;

    if (v2) valueFree(v2);

    v2 = doAddSubtract(state);
    if (v2 == NULL)
      return NULL;

    if (! valueSameType(v1, v2)) {
      rpmError(RPMERR_BADSPEC, _("types must match"));
      return NULL;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i, r = 0;
      switch (op) {
      case TOK_EQ:
	r = (i1 == i2);
	break;
      case TOK_NEQ:
	r = (i1 != i2);
	break;
      case TOK_LT:
	r = (i1 < i2);
	break;
      case TOK_LE:
	r = (i1 <= i2);
	break;
      case TOK_GT:
	r = (i1 > i2);
	break;
      case TOK_GE:
	r = (i1 >= i2);
	break;
      default:
	break;
      }
      valueFree(v1);
      v1 = valueMakeInteger(r);
    } else {
      char *s1 = v1->data.s, *s2 = v2->data.s, r = 0;
      switch (op) {
      case TOK_EQ:
	r = (strcmp(s1,s2) == 0);
	break;
      case TOK_NEQ:
	r = (strcmp(s1,s2) != 0);
	break;
      case TOK_LT:
	r = (strcmp(s1,s2) < 0);
	break;
      case TOK_LE:
	r = (strcmp(s1,s2) <= 0);
	break;
      case TOK_GT:
	r = (strcmp(s1,s2) > 0);
	break;
      case TOK_GE:
	r = (strcmp(s1,s2) >= 0);
	break;
      default:
	break;
      }
      valueFree(v1);
      v1 = valueMakeInteger(r);
    }
  }

  if (v2) valueFree(v2);
  return v1;
}

static Value doLogical(ParseState state)
{
  Value v1, v2 = NULL;

  DEBUG(printf("doLogical()\n"));

  v1 = doRelational(state);
  if (v1 == NULL)
    return NULL;

  while (state->nextToken == TOK_LOGICAL_AND
	 || state->nextToken == TOK_LOGICAL_OR) {
    int op = state->nextToken;

    if (rdToken(state))
      return NULL;

    if (v2) valueFree(v2);

    v2 = doRelational(state);
    if (v2 == NULL)
      return NULL;

    if (! valueSameType(v1, v2)) {
      rpmError(RPMERR_BADSPEC, _("types must match"));
      return NULL;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      valueFree(v1);
      if (op == TOK_LOGICAL_AND)
	v1 = valueMakeInteger(i1 && i2);
      else
	v1 = valueMakeInteger(i1 || i2);
    } else {
      rpmError(RPMERR_BADSPEC, _("&& and || not suported for strings"));
      return NULL;
    }
  }

  if (v2) valueFree(v2);
  return v1;
}

int parseExpressionBoolean(Spec spec, char *expr)
{
  struct _parseState state;
  int result = -1;
  Value v;

  DEBUG(printf("parseExprBoolean(?, '%s')\n", expr));

  /* Initialize the expression parser state. */
  state.p = state.str = xstrdup(expr);
  state.spec = spec;
  state.nextToken = 0;
  state.tokenValue = NULL;
  rdToken(&state);

  /* Parse the expression. */
  v = doLogical(&state);
  if (!v) {
    free(state.str);
    return -1;
  }

  /* If the next token is not TOK_EOF, we have a syntax error. */
  if (state.nextToken != TOK_EOF) {
    rpmError(RPMERR_BADSPEC, _("syntax error in expression"));
    free(state.str);
    return -1;
  }

  DEBUG(valueDump("parseExprBoolean:", v, stdout));

  switch (v->type) {
  case VALUE_TYPE_INTEGER:
    result = v->data.i != 0;
    break;
  case VALUE_TYPE_STRING:
    result = v->data.s[0] != '\0';
    break;
  default:
    break;
  }

  free(state.str);
  valueFree(v);
  return result;
}

char * parseExpressionString(Spec spec, char *expr)
{
  struct _parseState state;
  char *result = NULL;
  Value v;

  DEBUG(printf("parseExprString(?, '%s')\n", expr));

  /* Initialize the expression parser state. */
  state.p = state.str = xstrdup(expr);
  state.spec = spec;
  state.nextToken = 0;
  state.tokenValue = NULL;
  rdToken(&state);

  /* Parse the expression. */
  v = doLogical(&state);
  if (!v) {
    free(state.str);
    return NULL;
  }

  /* If the next token is not TOK_EOF, we have a syntax error. */
  if (state.nextToken != TOK_EOF) {
    rpmError(RPMERR_BADSPEC, _("syntax error in expression"));
    free(state.str);
    return NULL;
  }

  DEBUG(valueDump("parseExprString:", v, stdout));

  switch (v->type) {
  case VALUE_TYPE_INTEGER: {
    char buf[128];
    sprintf(buf, "%d", v->data.i);
    result = xstrdup(buf);
  } break;
  case VALUE_TYPE_STRING:
    result = xstrdup(v->data.s);
    break;
  default:
    break;
  }

  free(state.str);
  valueFree(v);
  return result;
}
