/* lrexlib.c - POSIX & PCRE regular expression library */
/* POSIX regexs can use Spencer extensions for matching NULs if available
   (REG_BASIC) */
/* Reuben Thomas   nov00-06oct03 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

/*@access regex_t @*/

/* Sanity check */
#if !defined(WITH_POSIX) && !defined(WITH_PCRE)
#error Define WITH_POSIX or WITH_PCRE, otherwise this library is useless!
#endif


/* POSIX regex methods */

#ifdef WITH_POSIX

#include <regex.h>

static int rex_comp(lua_State *L)
	/*@modifies L @*/
{
  size_t l;
  const char *pattern;
  int res;
  regex_t *pr = (regex_t *)lua_newuserdata(L, sizeof(regex_t));
  pattern = luaL_checklstring(L, 1, &l);
#ifdef REG_BASIC
  pr->re_endp = pattern + lua_strlen(L, 1);
  res = regcomp(pr, pattern, REG_EXTENDED | REG_PEND);
#else
  res = regcomp(pr, pattern, REG_EXTENDED);
#endif
  if (res) {
    size_t sz = regerror(res, pr, NULL, 0);
    char errbuf[sz];
    regerror(res, pr, errbuf, sz);
    lua_pushstring(L, errbuf);
    lua_error(L);
  }
  luaL_getmetatable(L, "regex_t");
  lua_setmetatable(L, -2);
  return 1;
}

static void rex_getargs(lua_State *L, /*@unused@*/ size_t *len, size_t *ncapt,
                        const char **text, regex_t **pr, regmatch_t **match)
	/*@modifies L, *ncapt, *text, *pr, *match @*/
{
  luaL_checkany(L, 1);
  *pr = (regex_t *)lua_touserdata(L, 1);
/*@-observertrans -dependenttrans @*/
#ifdef REG_BASIC
  *text = luaL_checklstring(L, 2, len);
#else
  *text = luaL_checklstring(L, 2, NULL);
#endif
/*@=observertrans =dependenttrans @*/
  *ncapt = (*pr)->re_nsub;
  luaL_checkstack(L, *ncapt + 2, "too many captures");
  *match = malloc((*ncapt + 1) * sizeof(regmatch_t));
}

static void rex_push_matches(lua_State *L, const char *text, regmatch_t *match,
                             size_t ncapt)
	/*@modifies L @*/
{
  size_t i;
  lua_newtable(L);
  for (i = 1; i <= ncapt; i++) {
    if (match[i].rm_so >= 0) {
      lua_pushlstring(L, text + match[i].rm_so,
                      match[i].rm_eo - match[i].rm_so);
      lua_rawseti(L, -2, i);
    }
  }
}

static int rex_match(lua_State *L)
	/*@modifies L @*/
{
  int res;
#ifdef REG_BASIC
  size_t len;
#endif
  size_t ncapt;
  const char *text;
  regex_t *pr;
  regmatch_t *match;
  rex_getargs(L,
#ifdef REG_BASIC
          &len,
#else
          NULL,
#endif
          &ncapt, &text, &pr, &match);
#ifdef REG_BASIC
  match[0].rm_so = 0;
  match[0].rm_eo = len;
  res = regexec(pr, text, ncapt + 1, match, REG_STARTEND);
#else
  res = regexec(pr, text, ncapt + 1, match, 0);
#endif
  if (res == 0) {
    lua_pushnumber(L, match[0].rm_so + 1);
    lua_pushnumber(L, match[0].rm_eo);
    rex_push_matches(L, text, match, ncapt);
    lua_pushstring(L, "n");
    lua_pushnumber(L, ncapt);
    lua_rawset(L, -3);
    return 3;
  } else
    return 0;
}

static int rex_gmatch(lua_State *L)
	/*@modifies L @*/
{
  int res;
#ifdef REG_BASIC
  size_t len;
#endif
  size_t ncapt, nmatch = 0, maxmatch = 0, limit = 0;
  const char *text;
  regex_t *pr;
  regmatch_t *match;
  rex_getargs(L,
#ifdef REG_BASIC
          &len,
#else
          NULL,
#endif
          &ncapt, &text, &pr, &match);
  luaL_checktype(L, 3, LUA_TFUNCTION);
  if (lua_gettop(L) > 3) {
    maxmatch = (size_t)luaL_checknumber(L, 4);
    limit = 1;
  }
  while (!limit || nmatch < maxmatch) {
#ifdef REG_BASIC
    match[0].rm_so = 0;
    match[0].rm_eo = len;
    res = regexec(pr, text, ncapt + 1, match, REG_STARTEND);
#else
    res = regexec(pr, text, ncapt + 1, match, 0);
#endif
    if (res == 0) {
      lua_pushvalue(L, 3);
      lua_pushlstring(L, text + match[0].rm_so, match[0].rm_eo - match[0].rm_so);
      rex_push_matches(L, text, match, ncapt);
      lua_call(L, 2, 0);
      text += match[0].rm_eo;
#ifdef REG_BASIC
      len -= match[0].rm_eo;
#endif
      nmatch++;
    } else
      break;
  }
  lua_pushnumber(L, nmatch);
  return 1;
}

static int rex_gc (lua_State *L)
	/*@modifies L @*/
{
  regex_t *r = (regex_t *)luaL_checkudata(L, 1, "regex_t");
  if (r)
    regfree(r);
  return 0;
}

/*@-readonlytrans@*/
/*@unchecked@*/
static const luaL_reg rexmeta[] = {
  {"match",   rex_match},
  {"gmatch",  rex_gmatch},
  {"__gc",    rex_gc},
  {NULL, NULL}
};
/*@=readonlytrans@*/

#endif /* WITH_POSIX */


/* PCRE methods */

#ifdef WITH_PCRE

#include <pcre/pcre.h>

static int pcre_comp(lua_State *L)
{
  size_t l;
  const char *pattern;
  const char *error;
  int erroffset;
  pcre **ppr = (pcre **)lua_newuserdata(L, sizeof(pcre **));
  pcre *pr;
  pattern = luaL_checklstring(L, 1, &l);
  pr = pcre_compile(pattern, 0, &error, &erroffset, NULL);
  if (!pr) {
    lua_pushstring(L, error);
    lua_error(L);
  }
  *ppr = pr;
  luaL_getmetatable(L, "pcre");
  lua_setmetatable(L, -2);
  return 1;
}

static void pcre_getargs(lua_State *L, int *len, int *ncapt, const char **text,
                        pcre ***ppr, int **match)
{
  luaL_checkany(L, 1);
  *ppr = (pcre **)lua_touserdata(L, 1);
  *text = luaL_checklstring(L, 2, len);
  pcre_fullinfo(**ppr, NULL, PCRE_INFO_CAPTURECOUNT, ncapt);
  luaL_checkstack(L, *ncapt + 2, "too many captures");
  /* need (2 ints per capture, plus one for substring match) * 3/2 */
  *match = malloc((*ncapt + 1) * 3 * sizeof(int));
}

static void pcre_push_matches(lua_State *L, const char *text, int *match,
                             int ncapt)
{
  int i;
  lua_newtable(L);
  for (i = 1; i <= ncapt; i++) {
    if (match[i * 2] >= 0) {
      lua_pushlstring(L, text + match[i * 2],
                      match[i * 2 + 1] - match[i * 2]);
      lua_rawseti(L, -2, i);
    }
  }
}

static int pcre_match(lua_State *L)
{
  int res;
  const char *text;
  pcre **ppr;
  int *match;
  int ncapt;
  int len;
  pcre_getargs(L, &len, &ncapt, &text, &ppr, &match);
  res = pcre_exec(*ppr, NULL, text, len, 0, 0, match, (ncapt + 1) * 3);
  if (res >= 0) {
    lua_pushnumber(L, match[0] + 1);
    lua_pushnumber(L, match[1]);
    pcre_push_matches(L, text, match, ncapt);
    lua_pushstring(L, "n");
    lua_pushnumber(L, ncapt);
    lua_rawset(L, -3);
    return 3;
  } else
    return 0;
}

static int pcre_gmatch(lua_State *L)
{
  int res;
  const char *text;
  int limit = 0;
  int ncapt, nmatch = 0, maxmatch;
  pcre **ppr;
  int *match;
  int len;
  pcre_getargs(L, &len, &ncapt, &text, &ppr, &match);
  luaL_checktype(L, 3, LUA_TFUNCTION);
  if (lua_gettop(L) > 3) {
    maxmatch = (int)luaL_checknumber(L, 4);
    limit = 1;
  }
  while (!limit || nmatch < maxmatch) {
    res = pcre_exec(*ppr, NULL, text, len, 0, 0, match, (ncapt + 1) * 3);
    if (res == 0) {
      lua_pushvalue(L, 3);
      lua_pushlstring(L, text + match[0], match[1] - match[0]);
      pcre_push_matches(L, text, match, ncapt);
      lua_call(L, 2, 0);
      text += match[1];
      len -= match[1];
      nmatch++;
    } else
      break;
  }
  lua_pushnumber(L, nmatch);
  return 1;
}

static int pcre_gc (lua_State *L)
{
  pcre **ppr = (pcre **)luaL_checkudata(L, 1, "pcre");
  if (ppr)
    pcre_free(*ppr);
  return 0;
}

static const luaL_reg pcremeta[] = {
  {"match",  pcre_match},
  {"gmatch", pcre_gmatch},
  {"__gc",   pcre_gc},
  {NULL, NULL}
};

#endif /* defined(WITH_PCRE) */


/* Open the library */

/*@-readonlytrans@*/
/*@unchecked@*/
static const luaL_reg rexlib[] = {
#ifdef WITH_POSIX
  {"newPOSIX", rex_comp},
#endif
#ifdef WITH_PCRE
  {"newPCRE", pcre_comp},
#endif
  {NULL, NULL}
};
/*@=readonlytrans@*/

static void createmeta(lua_State *L, const char *name)
	/*@modifies L @*/
{
  luaL_newmetatable(L, name);   /* create new metatable */
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);         /* push metatable */
  lua_rawset(L, -3);            /* metatable.__index = metatable */
}

LUALIB_API int luaopen_rex(lua_State *L)
{
#ifdef WITH_POSIX
  createmeta(L, "regex_t");
  luaL_openlib(L, NULL, rexmeta, 0);
  lua_pop(L, 1);
#endif
#ifdef WITH_PCRE
  createmeta(L, "pcre");
  luaL_openlib(L, NULL, pcremeta, 0);
  lua_pop(L, 1);
#endif
  luaL_openlib(L, "rex", rexlib, 0);
  return 1;
}
