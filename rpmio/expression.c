/** \ingroup rpmbuild
 * \file build/expression.c
 *  Simple logical expression parser.
 * This module implements a basic expression parser with support for
 * integer and string datatypes. For ease of programming, we use the
 * top-down "recursive descent" method of parsing. While a
 * table-driven bottom-up parser might be faster, it does not really
 * matter for the expressions we will be parsing.
 *
 * Copyright (C) 1998 Tom Dyas <tdyas@eden.rutgers.edu>
 * This work is provided under the GPL or LGPL at your choice.
 */

#include "system.h"

#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include "debug.h"

/* #define DEBUG_PARSER 1 */

#ifdef DEBUG_PARSER
#include <stdio.h>
#define DEBUG(x) do { x ; } while (0)
#else
#define DEBUG(x)
#endif

typedef enum { 
    VALUE_TYPE_INTEGER,
    VALUE_TYPE_STRING,
} valueType;

/**
 * Encapsulation of a "value"
 */
typedef struct _value {
  valueType type;
  union {
    char *s;
    int i;
  } data;
} *Value;

/**
 */
static Value valueMakeInteger(int i)
{
  Value v;

  v = (Value) xmalloc(sizeof(*v));
  v->type = VALUE_TYPE_INTEGER;
  v->data.i = i;
  return v;
}

/**
 */
static Value valueMakeString(char *s)
{
  Value v;

  v = (Value) xmalloc(sizeof(*v));
  v->type = VALUE_TYPE_STRING;
  v->data.s = s;
  return v;
}

/**
 */
static void valueSetInteger(Value v, int i)
{
  if (v->type == VALUE_TYPE_STRING)
    v->data.s = _free(v->data.s);
  v->type = VALUE_TYPE_INTEGER;
  v->data.i = i;
}

/**
 */
static void valueSetString(Value v, char *s)
{
  if (v->type == VALUE_TYPE_STRING)
    v->data.s = _free(v->data.s);
  v->type = VALUE_TYPE_STRING;
  v->data.s = s;
}

/**
 */
static void valueFree( Value v)
{
  if (v) {
    if (v->type == VALUE_TYPE_STRING)
	v->data.s = _free(v->data.s);
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


/**
 * Parser state.
 */
typedef struct _parseState {
    char *str;		/*!< expression string */
    const char *p;	/*!< current position in expression string */
    int nextToken;	/*!< current lookahead token */
    Value tokenValue;	/*!< valid when TOK_INTEGER or TOK_STRING */
} *ParseState;

static void exprErr(const struct _parseState *state, const char *msg,
		    const char *p)
{
    const char *newLine = strchr(state->str,'\n');

    if (newLine && (*(newLine+1) != '\0'))
	p = NULL;

    rpmlog(RPMLOG_ERR, "%s: %s\n", msg, state->str);
    if (p) {
	int l = p - state->str + strlen(msg) + 2;
	rpmlog(RPMLOG_ERR, "%*s\n", l, "^");
    }
}

/**
 * \name Parser tokens
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

/**
 * @param state		expression parser state
 */
static int rdToken(ParseState state)
{
  int token;
  Value v = NULL;
  const char *p = state->p;

  /* Skip whitespace before the next token. */
  while (*p && risspace(*p)) p++;

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
      exprErr(state, _("syntax error while parsing =="), p+2);
      goto err;
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
      exprErr(state, _("syntax error while parsing &&"), p+2);
      goto err;
    }
    break;
  case '|':
    if (p[1] == '|') {
      token = TOK_LOGICAL_OR;
      p++;
    } else {
      exprErr(state, _("syntax error while parsing ||"), p+2);
      goto err;
    }
    break;

  default:
    if (risdigit(*p)) {
      char *temp;
      size_t ts;

      for (ts=1; p[ts] && risdigit(p[ts]); ts++);
      temp = xmalloc(ts+1);
      memcpy(temp, p, ts);
      p += ts-1;
      temp[ts] = '\0';

      token = TOK_INTEGER;
      v = valueMakeInteger(atoi(temp));
      free(temp);

    } else if (risalpha(*p)) {
      char *temp;
      size_t ts;

      for (ts=1; p[ts] && (risalnum(p[ts]) || p[ts] == '_'); ts++);
      temp = xmalloc(ts+1);
      memcpy(temp, p, ts);
      p += ts-1;
      temp[ts] = '\0';

      token = TOK_IDENTIFIER;
      v = valueMakeString(temp);

    } else if (*p == '\"') {
      char *temp;
      size_t ts;

      p++;
      for (ts=0; p[ts] && p[ts] != '\"'; ts++);
      temp = xmalloc(ts+1);
      memcpy(temp, p, ts);
      p += ts-1;
      temp[ts] = '\0';
      p++;

      token = TOK_STRING;
      v = valueMakeString( rpmExpand(temp, NULL) );
      free(temp);

    } else {
      exprErr(state, _("parse error in expression"), p+1);
      goto err;
    }
  }

  state->p = p + 1;
  state->nextToken = token;
  state->tokenValue = v;

  DEBUG(printf("rdToken: \"%s\" (%d)\n", prToken(token), token));
  DEBUG(valueDump("rdToken:", state->tokenValue, stdout));

  return 0;

err:
  valueFree(v);
  return -1;
}

static Value doLogical(ParseState state);


/**
 * @param state		expression parser state
 */
static Value doPrimary(ParseState state)
{
  Value v = NULL;

  DEBUG(printf("doPrimary()\n"));

  switch (state->nextToken) {
  case TOK_OPEN_P:
    if (rdToken(state))
      goto err;
    v = doLogical(state);
    if (state->nextToken != TOK_CLOSE_P) {
      exprErr(state, _("unmatched ("), NULL);
      goto err;
    }
    if (rdToken(state))
      goto err;
    break;

  case TOK_INTEGER:
  case TOK_STRING:
    v = state->tokenValue;
    if (rdToken(state))
      goto err;
    break;

  case TOK_IDENTIFIER: {
    const char *name = state->tokenValue->data.s;

    v = valueMakeString( rpmExpand(name, NULL) );
    if (rdToken(state))
      goto err;
    break;
  }

  case TOK_MINUS:
    if (rdToken(state))
      goto err;

    v = doPrimary(state);
    if (v == NULL)
      goto err;

    if (! valueIsInteger(v)) {
      exprErr(state, _("- only on numbers"), NULL);
      goto err;
    }

    valueSetInteger(v, - v->data.i);
    break;

  case TOK_NOT:
    if (rdToken(state))
      goto err;

    v = doPrimary(state);
    if (v == NULL)
      goto err;

    if (! valueIsInteger(v)) {
      exprErr(state, _("! only on numbers"), NULL);
      goto err;
    }

    valueSetInteger(v, ! v->data.i);
    break;

  case TOK_EOF:
    exprErr(state, _("unexpected end of expression"), NULL);
    goto err;

  default:
    exprErr(state, _("syntax error in expression"), state->p);
    goto err;
    break;
  }

  DEBUG(valueDump("doPrimary:", v, stdout));
  return v;

err:
  valueFree(v);
  return NULL;
}

/**
 * @param state		expression parser state
 */
static Value doMultiplyDivide(ParseState state)
{
  Value v1 = NULL, v2 = NULL;

  DEBUG(printf("doMultiplyDivide()\n"));

  v1 = doPrimary(state);
  if (v1 == NULL)
    goto err;

  while (state->nextToken == TOK_MULTIPLY
	 || state->nextToken == TOK_DIVIDE) {
    int op = state->nextToken;

    if (rdToken(state))
      goto err;

    if (v2) valueFree(v2);

    v2 = doPrimary(state);
    if (v2 == NULL)
      goto err;

    if (! valueSameType(v1, v2)) {
      exprErr(state, _("types must match"), NULL);
      goto err;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      if ((i2 == 0) && (op == TOK_DIVIDE)) {
	    exprErr(state, _("division by zero"), NULL);
	    goto err;
      }
      if (op == TOK_MULTIPLY)
	valueSetInteger(v1, i1 * i2);
      else
	valueSetInteger(v1, i1 / i2);
    } else {
      exprErr(state, _("* and / not supported for strings"), NULL);
      goto err;
    }
  }

  if (v2) valueFree(v2);
  return v1;

err:
  valueFree(v1);
  valueFree(v2);
  return NULL;
}

/**
 * @param state		expression parser state
 */
static Value doAddSubtract(ParseState state)
{
  Value v1 = NULL, v2 = NULL;

  DEBUG(printf("doAddSubtract()\n"));

  v1 = doMultiplyDivide(state);
  if (v1 == NULL)
    goto err;

  while (state->nextToken == TOK_ADD || state->nextToken == TOK_MINUS) {
    int op = state->nextToken;

    if (rdToken(state))
      goto err;

    if (v2) valueFree(v2);

    v2 = doMultiplyDivide(state);
    if (v2 == NULL)
      goto err;

    if (! valueSameType(v1, v2)) {
      exprErr(state, _("types must match"), NULL);
      goto err;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      if (op == TOK_ADD)
	valueSetInteger(v1, i1 + i2);
      else
	valueSetInteger(v1, i1 - i2);
    } else {
      char *copy;

      if (op == TOK_MINUS) {
        exprErr(state, _("- not supported for strings"), NULL);
        goto err;
      }

      copy = xmalloc(strlen(v1->data.s) + strlen(v2->data.s) + 1);
      (void) stpcpy( stpcpy(copy, v1->data.s), v2->data.s);

      valueSetString(v1, copy);
    }
  }

  if (v2) valueFree(v2);
  return v1;

err:
  valueFree(v1);
  valueFree(v2);
  return NULL;
}

/**
 * @param state		expression parser state
 */
static Value doRelational(ParseState state)
{
  Value v1 = NULL, v2 = NULL;

  DEBUG(printf("doRelational()\n"));

  v1 = doAddSubtract(state);
  if (v1 == NULL)
    goto err;

  while (state->nextToken >= TOK_EQ && state->nextToken <= TOK_GE) {
    int op = state->nextToken;

    if (rdToken(state))
      goto err;

    if (v2) valueFree(v2);

    v2 = doAddSubtract(state);
    if (v2 == NULL)
      goto err;

    if (! valueSameType(v1, v2)) {
      exprErr(state, _("types must match"), NULL);
      goto err;
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
      valueSetInteger(v1, r);
    } else {
      const char * s1 = v1->data.s;
      const char * s2 = v2->data.s;
      int r = 0;
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
      valueSetInteger(v1, r);
    }
  }

  if (v2) valueFree(v2);
  return v1;

err:
  valueFree(v1);
  valueFree(v2);
  return NULL;
}

/**
 * @param state		expression parser state
 */
static Value doLogical(ParseState state)
{
  Value v1 = NULL, v2 = NULL;

  DEBUG(printf("doLogical()\n"));

  v1 = doRelational(state);
  if (v1 == NULL)
    goto err;

  while (state->nextToken == TOK_LOGICAL_AND
	 || state->nextToken == TOK_LOGICAL_OR) {
    int op = state->nextToken;

    if (rdToken(state))
      goto err;

    if (v2) valueFree(v2);

    v2 = doRelational(state);
    if (v2 == NULL)
      goto err;

    if (! valueSameType(v1, v2)) {
      exprErr(state, _("types must match"), NULL);
      goto err;
    }

    if (valueIsInteger(v1)) {
      int i1 = v1->data.i, i2 = v2->data.i;

      if (op == TOK_LOGICAL_AND)
	valueSetInteger(v1, i1 && i2);
      else
	valueSetInteger(v1, i1 || i2);
    } else {
      exprErr(state, _("&& and || not supported for strings"), NULL);
      goto err;
    }
  }

  if (v2) valueFree(v2);
  return v1;

err:
  valueFree(v1);
  valueFree(v2);
  return NULL;
}

int rpmExprBool(const char *expr)
{
  struct _parseState state;
  int result = -1;
  Value v = NULL;

  DEBUG(printf("parseExprBoolean(?, '%s')\n", expr));

  /* Initialize the expression parser state. */
  state.p = state.str = xstrdup(expr);
  state.nextToken = 0;
  state.tokenValue = NULL;
  if (rdToken(&state))
    goto exit;

  /* Parse the expression. */
  v = doLogical(&state);
  if (!v)
    goto exit;

  /* If the next token is not TOK_EOF, we have a syntax error. */
  if (state.nextToken != TOK_EOF) {
    exprErr(&state, _("syntax error in expression"), state.p);
    goto exit;
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

exit:
  state.str = _free(state.str);
  valueFree(v);
  return result;
}

char *rpmExprStr(const char *expr)
{
  struct _parseState state;
  char *result = NULL;
  Value v = NULL;

  DEBUG(printf("parseExprString(?, '%s')\n", expr));

  /* Initialize the expression parser state. */
  state.p = state.str = xstrdup(expr);
  state.nextToken = 0;
  state.tokenValue = NULL;
  if (rdToken(&state))
    goto exit;

  /* Parse the expression. */
  v = doLogical(&state);
  if (!v)
    goto exit;

  /* If the next token is not TOK_EOF, we have a syntax error. */
  if (state.nextToken != TOK_EOF) {
    exprErr(&state, _("syntax error in expression"), state.p);
    goto exit;
  }

  DEBUG(valueDump("parseExprString:", v, stdout));

  switch (v->type) {
  case VALUE_TYPE_INTEGER: {
    rasprintf(&result, "%d", v->data.i);
  } break;
  case VALUE_TYPE_STRING:
    result = xstrdup(v->data.s);
    break;
  default:
    break;
  }

exit:
  state.str = _free(state.str);
  valueFree(v);
  return result;
}
