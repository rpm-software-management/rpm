/* Copyright (c) 1998, 1999 Thai Open Source Software Center Ltd
   See the file COPYING for copying permission.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "expat.h"
#include "codepage.h"
#include "xmlfile.h"
#include "xmltchar.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

/* This ensures proper sorting. */

#define NSSEP T('\001')

static void
characterData(void *userData, const XML_Char *s, int len)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  FILE *fp = userData;
  for (; len > 0; --len, ++s) {
    switch (*s) {
    case T('&'):
      (void) fputts(T("&amp;"), fp);
      break;
    case T('<'):
      (void) fputts(T("&lt;"), fp);
      break;
    case T('>'):
      (void) fputts(T("&gt;"), fp);
      break;
#ifdef W3C14N
    case 13:
      (void) fputts(T("&#xD;"), fp);
      break;
#else
    case T('"'):
      (void) fputts(T("&quot;"), fp);
      break;
    case 9:
    case 10:
    case 13:
      ftprintf(fp, T("&#%d;"), *s);
      break;
#endif
    default:
      (void) puttc(*s, fp);
      break;
    }
  }
}

static void
attributeValue(FILE *fp, const XML_Char *s)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
{
  (void) puttc(T('='), fp);
  (void) puttc(T('"'), fp);
  for (;;) {
    switch (*s) {
    case 0:
    case NSSEP:
      (void) puttc(T('"'), fp);
      return;
    case T('&'):
      (void) fputts(T("&amp;"), fp);
      break;
    case T('<'):
      (void) fputts(T("&lt;"), fp);
      break;
    case T('"'):
      (void) fputts(T("&quot;"), fp);
      break;
#ifdef W3C14N
    case 9:
      (void) fputts(T("&#x9;"), fp);
      break;
    case 10:
      (void) fputts(T("&#xA;"), fp);
      break;
    case 13:
      (void) fputts(T("&#xD;"), fp);
      break;
#else
    case T('>'):
      (void) fputts(T("&gt;"), fp);
      break;
    case 9:
    case 10:
    case 13:
      ftprintf(fp, T("&#%d;"), *s);
      break;
#endif
    default:
      (void) puttc(*s, fp);
      break;
    }
    s++;
  }
}

/* Lexicographically comparing UTF-8 encoded attribute values,
is equivalent to lexicographically comparing based on the character number. */

static int
attcmp(const void *att1, const void *att2)
	/*@*/
{
  return tcscmp(*(const XML_Char **)att1, *(const XML_Char **)att2);
}

static void
startElement(void *userData, const XML_Char *name, const XML_Char **atts)
	/*@globals fileSystem @*/
	/*@modifies userData, *atts, fileSystem @*/
{
  int nAtts;
  const XML_Char **p;
  FILE *fp = userData;
  (void) puttc(T('<'), fp);
  (void) fputts(name, fp);

  p = atts;
  while (*p != NULL)
    ++p;
  nAtts = (p - atts) >> 1;
  if (nAtts > 1)
    qsort((void *)atts, nAtts, sizeof(XML_Char *) * 2, attcmp);
  while (*atts != NULL) {
    (void) puttc(T(' '), fp);
    (void) fputts(*atts++, fp);
    attributeValue(fp, *atts);
    atts++;
  }
  (void) puttc(T('>'), fp);
}

static void
endElement(void *userData, const XML_Char *name)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  FILE *fp = userData;
  (void) puttc(T('<'), fp);
  (void) puttc(T('/'), fp);
  (void) fputts(name, fp);
  (void) puttc(T('>'), fp);
}

static int
nsattcmp(const void *p1, const void *p2)
	/*@*/
{
  const XML_Char *att1 = *(const XML_Char **)p1;
  const XML_Char *att2 = *(const XML_Char **)p2;
  int sep1 = (tcsrchr(att1, NSSEP) != 0);
  int sep2 = (tcsrchr(att1, NSSEP) != 0);
  if (sep1 != sep2)
    return sep1 - sep2;
  return tcscmp(att1, att2);
}

static void
startElementNS(void *userData, const XML_Char *name, const XML_Char **atts)
	/*@globals fileSystem @*/
	/*@modifies userData, *atts, fileSystem @*/
{
  int nAtts;
  int nsi;
  const XML_Char **p;
  FILE *fp = userData;
  const XML_Char *sep;
  (void) puttc(T('<'), fp);

  sep = tcsrchr(name, NSSEP);
  if (sep != NULL) {
    (void) fputts(T("n1:"), fp);
    (void) fputts(sep + 1, fp);
    (void) fputts(T(" xmlns:n1"), fp);
    attributeValue(fp, name);
    nsi = 2;
  }
  else {
    (void) fputts(name, fp);
    nsi = 1;
  }

  p = atts;
  while (*p != NULL)
    ++p;
  nAtts = (p - atts) >> 1;
  if (nAtts > 1)
    qsort((void *)atts, nAtts, sizeof(XML_Char *) * 2, nsattcmp);
  while (*atts != NULL) {
    name = *atts++;
    sep = tcsrchr(name, NSSEP);
    (void) puttc(T(' '), fp);
    if (sep != NULL) {
      ftprintf(fp, T("n%d:"), nsi);
      (void) fputts(sep + 1, fp);
    }
    else
      (void) fputts(name, fp);
    attributeValue(fp, *atts);
    if (sep != NULL) {
      ftprintf(fp, T(" xmlns:n%d"), nsi++);
      attributeValue(fp, name);
    }
    atts++;
  }
  (void) puttc(T('>'), fp);
}

static void
endElementNS(void *userData, const XML_Char *name)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  FILE *fp = userData;
  const XML_Char *sep;
  (void) puttc(T('<'), fp);
  (void) puttc(T('/'), fp);
  sep = tcsrchr(name, NSSEP);
  if (sep != NULL) {
    (void) fputts(T("n1:"), fp);
    (void) fputts(sep + 1, fp);
  }
  else
    (void) fputts(name, fp);
  (void) puttc(T('>'), fp);
}

#ifndef W3C14N

static void
processingInstruction(void *userData, const XML_Char *target,
                      const XML_Char *data)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  FILE *fp = userData;
  (void) puttc(T('<'), fp);
  (void) puttc(T('?'), fp);
  (void) fputts(target, fp);
  (void) puttc(T(' '), fp);
  (void) fputts(data, fp);
  (void) puttc(T('?'), fp);
  (void) puttc(T('>'), fp);
}

#endif /* not W3C14N */

static void
defaultCharacterData(void *userData, /*@unused@*/ const XML_Char *s,
		/*@unused@*/ int len)
	/*@modifies userData @*/
{
  XML_DefaultCurrent((XML_Parser) userData);
}

static void
defaultStartElement(void *userData, /*@unused@*/ const XML_Char *name,
                    /*@unused@*/ const XML_Char **atts)
	/*@modifies userData @*/
{
  XML_DefaultCurrent((XML_Parser) userData);
}

static void
defaultEndElement(void *userData, /*@unused@*/ const XML_Char *name)
	/*@modifies userData @*/
{
  XML_DefaultCurrent((XML_Parser) userData);
}

static void
defaultProcessingInstruction(void *userData,
		/*@unused@*/ const XML_Char *target,
		/*@unused@*/ const XML_Char *data)
	/*@modifies userData @*/
{
  XML_DefaultCurrent((XML_Parser) userData);
}

/*@-paramuse@*/
static void
nopCharacterData(void *userData, const XML_Char *s, int len)
	/*@*/
{
}

static void
nopStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
	/*@*/
{
}

static void
nopEndElement(void *userData, const XML_Char *name)
	/*@*/
{
}

static void
nopProcessingInstruction(void *userData, const XML_Char *target,
                         const XML_Char *data)
	/*@*/
{
}
/*@=paramuse@*/

static void
markup(void *userData, const XML_Char *s, int len)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  FILE *fp = XML_GetUserData((XML_Parser) userData);
  for (; len > 0; --len, ++s)
    (void) puttc(*s, fp);
}

static void
metaLocation(XML_Parser parser)
	/*@globals fileSystem @*/
	/*@modifies parser, fileSystem @*/
{
  const XML_Char *uri = XML_GetBase(parser);
  if (uri != NULL)
    ftprintf(XML_GetUserData(parser), T(" uri=\"%s\""), uri);
  ftprintf(XML_GetUserData(parser),
           T(" byte=\"%ld\" nbytes=\"%d\" line=\"%d\" col=\"%d\""),
           XML_GetCurrentByteIndex(parser),
           XML_GetCurrentByteCount(parser),
           XML_GetCurrentLineNumber(parser),
           XML_GetCurrentColumnNumber(parser));
}

static void
metaStartDocument(void *userData)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  (void) fputts(T("<document>\n"), XML_GetUserData((XML_Parser) userData));
}

static void
metaEndDocument(void *userData)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  (void) fputts(T("</document>\n"), XML_GetUserData((XML_Parser) userData));
}

static void
metaStartElement(void *userData, const XML_Char *name,
                 const XML_Char **atts)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  const XML_Char **specifiedAttsEnd
    = atts + XML_GetSpecifiedAttributeCount(parser);
  const XML_Char **idAttPtr;
  int idAttIndex = XML_GetIdAttributeIndex(parser);
  if (idAttIndex < 0)
    idAttPtr = 0;
  else
    idAttPtr = atts + idAttIndex;
    
  ftprintf(fp, T("<starttag name=\"%s\""), name);
  metaLocation(parser);
  if (*atts != NULL) {
    (void) fputts(T(">\n"), fp);
    do {
      ftprintf(fp, T("<attribute name=\"%s\" value=\""), atts[0]);
      characterData(fp, atts[1], tcslen(atts[1]));
      if (atts >= specifiedAttsEnd)
        (void) fputts(T("\" defaulted=\"yes\"/>\n"), fp);
      else if (atts == idAttPtr)
        (void) fputts(T("\" id=\"yes\"/>\n"), fp);
      else
        (void) fputts(T("\"/>\n"), fp);
    } while (*(atts += 2) != NULL);
    (void) fputts(T("</starttag>\n"), fp);
  }
  else
    (void) fputts(T("/>\n"), fp);
}

static void
metaEndElement(void *userData, const XML_Char *name)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<endtag name=\"%s\""), name);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaProcessingInstruction(void *userData, const XML_Char *target,
                          const XML_Char *data)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<pi target=\"%s\" data=\""), target);
  characterData(fp, data, tcslen(data));
  (void) puttc(T('"'), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaComment(void *userData, const XML_Char *data)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<comment data=\""), fp);
  characterData(fp, data, tcslen(data));
  (void) puttc(T('"'), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaStartCdataSection(void *userData)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<startcdata"), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaEndCdataSection(void *userData)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<endcdata"), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaCharacterData(void *userData, const XML_Char *s, int len)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<chars str=\""), fp);
  characterData(fp, s, len);
  (void) puttc(T('"'), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaStartDoctypeDecl(void *userData,
                     const XML_Char *doctypeName,
                     /*@unused@*/ const XML_Char *sysid,
                     /*@unused@*/ const XML_Char *pubid,
                     /*@unused@*/ int has_internal_subset)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<startdoctype name=\"%s\""), doctypeName);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaEndDoctypeDecl(void *userData)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<enddoctype"), fp);
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}

static void
metaNotationDecl(void *userData,
                 const XML_Char *notationName,
                 /*@unused@*/ const XML_Char *base,
                 const XML_Char *systemId,
                 const XML_Char *publicId)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<notation name=\"%s\""), notationName);
  if (publicId != NULL)
    ftprintf(fp, T(" public=\"%s\""), publicId);
  if (systemId != NULL) {
    (void) fputts(T(" system=\""), fp);
    characterData(fp, systemId, tcslen(systemId));
    (void) puttc(T('"'), fp);
  }
  metaLocation(parser);
  (void) fputts(T("/>\n"), fp);
}


static void
metaEntityDecl(void *userData,
               const XML_Char *entityName,
               /*@unused@*/ int  is_param,
               const XML_Char *value,
               int  value_length,
               /*@unused@*/ const XML_Char *base,
               const XML_Char *systemId,
               const XML_Char *publicId,
               const XML_Char *notationName)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);

  if (value != NULL) {
    ftprintf(fp, T("<entity name=\"%s\""), entityName);
    metaLocation(parser);
    (void) puttc(T('>'), fp);
    characterData(fp, value, value_length);
    (void) fputts(T("</entity/>\n"), fp);
  }
  else if (notationName != NULL) {
    ftprintf(fp, T("<entity name=\"%s\""), entityName);
    if (publicId != NULL)
      ftprintf(fp, T(" public=\"%s\""), publicId);
    (void) fputts(T(" system=\""), fp);
    characterData(fp, systemId, tcslen(systemId));
    (void) puttc(T('"'), fp);
    ftprintf(fp, T(" notation=\"%s\""), notationName);
    metaLocation(parser);
    (void) fputts(T("/>\n"), fp);
  }
  else {
    ftprintf(fp, T("<entity name=\"%s\""), entityName);
    if (publicId != NULL)
      ftprintf(fp, T(" public=\"%s\""), publicId);
    (void) fputts(T(" system=\""), fp);
    characterData(fp, systemId, tcslen(systemId));
    (void) puttc(T('"'), fp);
    metaLocation(parser);
    (void) fputts(T("/>\n"), fp);
  }
}

static void
metaStartNamespaceDecl(void *userData,
                       const XML_Char *prefix,
                       const XML_Char *uri)
	/*@globals fileSystem @*/
	/*@modifies userData, fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  (void) fputts(T("<startns"), fp);
  if (prefix != NULL)
    ftprintf(fp, T(" prefix=\"%s\""), prefix);
  if (uri != NULL) {
    (void) fputts(T(" ns=\""), fp);
    characterData(fp, uri, tcslen(uri));
    (void) fputts(T("\"/>\n"), fp);
  }
  else
    (void) fputts(T("/>\n"), fp);
}

static void
metaEndNamespaceDecl(void *userData, const XML_Char *prefix)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  XML_Parser parser = (XML_Parser) userData;
  FILE *fp = XML_GetUserData(parser);
  if (prefix == NULL)
    (void) fputts(T("<endns/>\n"), fp);
  else
    ftprintf(fp, T("<endns prefix=\"%s\"/>\n"), prefix);
}

static int
unknownEncodingConvert(void *data, const char *p)
	/*@*/
{
  return codepageConvert(*(int *)data, p);
}

static int
unknownEncoding(/*@unused@*/ void *userData, const XML_Char *name,
		XML_Encoding *info)
	/*@modifies info @*/
{
  int cp;
  static const XML_Char prefixL[] = T("windows-");
  static const XML_Char prefixU[] = T("WINDOWS-");
  int i;

  for (i = 0; prefixU[i]; i++)
    if (name[i] != prefixU[i] && name[i] != prefixL[i])
      return 0;
  
  cp = 0;
  for (; name[i]; i++) {
    static const XML_Char digits[] = T("0123456789");
    const XML_Char *s = tcschr(digits, name[i]);
    if (s == NULL)
      return 0;
    cp *= 10;
    cp += s - digits;
    if (cp >= 0x10000)
      return 0;
  }
  if (!codepageMap(cp, info->map))
    return 0;
  info->convert = unknownEncodingConvert;
  /* We could just cast the code page integer to a void *,
  and avoid the use of release. */
/*@-type@*/
  info->release = free;
/*@=type@*/
  info->data = malloc(sizeof(int));
/*@-compdef@*/
  if (info->data == NULL)
    return 0;
  *(int *)info->data = cp;
  return 1;
/*@=compdef@*/
}

static int
notStandalone(/*@unused@*/ void *userData)
	/*@*/
{
  return 0;
}

static void
showVersion(XML_Char *prog)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  XML_Char *s = prog;
  XML_Char ch;
  const XML_Feature *features = XML_GetFeatureList();
  while ((ch = *s) != 0) {
    if (ch == '/'
#ifdef WIN32
        || ch == '\\'
#endif
        )
      prog = s + 1;
    ++s;
  }
  ftprintf(stdout, T("%s using %s\n"), prog, XML_ExpatVersion());
  if (features != NULL && features[0].feature != XML_FEATURE_END) {
    int i = 1;
    ftprintf(stdout, T("%s"), features[0].name);
    if (features[0].value)
      ftprintf(stdout, T("=%ld"), features[0].value);
    while (features[i].feature != XML_FEATURE_END) {
      ftprintf(stdout, T(", %s"), features[i].name);
      if (features[i].value)
        ftprintf(stdout, T("=%ld"), features[i].value);
      ++i;
    }
    ftprintf(stdout, T("\n"));
  }
}

/*@exits@*/
static void
usage(const XML_Char *prog, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
  ftprintf(stderr,
           T("usage: %s [-n] [-p] [-r] [-s] [-w] [-x] [-d output-dir] "
             "[-e encoding] file ...\n"), prog);
  exit(rc);
}

int
tmain(int argc, char *argv[])
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
  int i, j;
  const XML_Char *outputDir = NULL;
  const XML_Char *encoding = NULL;
  unsigned processFlags = XML_MAP_FILE;
  int windowsCodePages = 0;
  int outputType = 0;
  int useNamespaces = 0;
  int requireStandalone = 0;
  int paramEntityParsing = XML_PARAM_ENTITY_PARSING_NEVER;
  int useStdin = 0;

#ifdef _MSC_VER
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif

  i = 1;
  j = 0;
  while (i < argc) {
    if (j == 0) {
      if (argv[i][0] != T('-'))
        break;
      if (argv[i][1] == T('-') && argv[i][2] == T('\0')) {
        i++;
        break;
      }
      j++;
    }
    switch (argv[i][j]) {
    case T('r'):
      processFlags &= ~XML_MAP_FILE;
      j++;
      break;
    case T('s'):
      requireStandalone = 1;
      j++;
      break;
    case T('n'):
      useNamespaces = 1;
      j++;
      break;
    case T('p'):
      paramEntityParsing = XML_PARAM_ENTITY_PARSING_ALWAYS;
      /*@fallthrough@*/
    case T('x'):
      processFlags |= XML_EXTERNAL_ENTITIES;
      j++;
      break;
    case T('w'):
      windowsCodePages = 1;
      j++;
      break;
    case T('m'):
      outputType = 'm';
      j++;
      break;
    case T('c'):
      outputType = 'c';
      useNamespaces = 0;
      j++;
      break;
    case T('t'):
      outputType = 't';
      j++;
      break;
    case T('d'):
      if (argv[i][j + 1] == T('\0')) {
        if (++i == argc)
          usage(argv[0], 2);
        outputDir = argv[i];
      }
      else
        outputDir = argv[i] + j + 1;
      i++;
      j = 0;
      break;
    case T('e'):
      if (argv[i][j + 1] == T('\0')) {
        if (++i == argc)
          usage(argv[0], 2);
        encoding = argv[i];
      }
      else
        encoding = argv[i] + j + 1;
      i++;
      j = 0;
      break;
    case T('h'):
      usage(argv[0], 0);
      /*@notreached@*/
      return 0;
    case T('v'):
      showVersion(argv[0]);
      return 0;
    case T('\0'):
      if (j > 1) {
        i++;
        j = 0;
        break;
      }
      /*@fallthrough@*/
    default:
      usage(argv[0], 2);
    }
  }
  if (i == argc) {
    useStdin = 1;
    processFlags &= ~XML_MAP_FILE;
    i--;
  }
  for (; i < argc; i++) {
    FILE *fp = 0;
    XML_Char *outName = 0;
    int result;
    XML_Parser parser;
    if (useNamespaces)
      parser = XML_ParserCreateNS(encoding, NSSEP);
    else
      parser = XML_ParserCreate(encoding);
    if (requireStandalone)
      XML_SetNotStandaloneHandler(parser, notStandalone);
    (void) XML_SetParamEntityParsing(parser, paramEntityParsing);
    if (outputType == 't') {
      /* This is for doing timings; this gives a more realistic estimate of
         the parsing time. */
      outputDir = 0;
      XML_SetElementHandler(parser, nopStartElement, nopEndElement);
      XML_SetCharacterDataHandler(parser, nopCharacterData);
      XML_SetProcessingInstructionHandler(parser, nopProcessingInstruction);
    }
    else if (outputDir != NULL) {
      const XML_Char *file = useStdin ? T("STDIN") : argv[i];
      if (tcsrchr(file, T('/')) != NULL)
        file = tcsrchr(file, T('/')) + 1;
#ifdef WIN32
      if (tcsrchr(file, T('\\')))
        file = tcsrchr(file, T('\\')) + 1;
#endif
      outName = malloc((tcslen(outputDir) + tcslen(file) + 2)
                       * sizeof(XML_Char));
      tcscpy(outName, outputDir);
      tcscat(outName, T("/"));
      tcscat(outName, file);
      fp = tfopen(outName, T("wb"));
      if (fp == NULL) {
        tperror(outName);
        exit(EXIT_FAILURE);
      }
      (void) setvbuf(fp, NULL, _IOFBF, 16384);
#ifdef XML_UNICODE
      (void) puttc(0xFEFF, fp);
#endif
      XML_SetUserData(parser, fp);
      switch (outputType) {
      case 'm':
        XML_UseParserAsHandlerArg(parser);
        XML_SetElementHandler(parser, metaStartElement, metaEndElement);
        XML_SetProcessingInstructionHandler(parser, metaProcessingInstruction);
        XML_SetCommentHandler(parser, metaComment);
        XML_SetCdataSectionHandler(parser, metaStartCdataSection,
                                   metaEndCdataSection);
        XML_SetCharacterDataHandler(parser, metaCharacterData);
        XML_SetDoctypeDeclHandler(parser, metaStartDoctypeDecl,
                                  metaEndDoctypeDecl);
        XML_SetEntityDeclHandler(parser, metaEntityDecl);
        XML_SetNotationDeclHandler(parser, metaNotationDecl);
        XML_SetNamespaceDeclHandler(parser, metaStartNamespaceDecl,
                                    metaEndNamespaceDecl);
        metaStartDocument(parser);
        break;
      case 'c':
        XML_UseParserAsHandlerArg(parser);
        XML_SetDefaultHandler(parser, markup);
        XML_SetElementHandler(parser, defaultStartElement, defaultEndElement);
        XML_SetCharacterDataHandler(parser, defaultCharacterData);
        XML_SetProcessingInstructionHandler(parser,
                                            defaultProcessingInstruction);
        break;
      default:
        if (useNamespaces)
          XML_SetElementHandler(parser, startElementNS, endElementNS);
        else
          XML_SetElementHandler(parser, startElement, endElement);
        XML_SetCharacterDataHandler(parser, characterData);
#ifndef W3C14N
        XML_SetProcessingInstructionHandler(parser, processingInstruction);
#endif /* not W3C14N */
        break;
      }
    }
    if (windowsCodePages)
      XML_SetUnknownEncodingHandler(parser, unknownEncoding, 0);
    result = XML_ProcessFile(parser, useStdin ? NULL : argv[i], processFlags);
/*@-branchstate@*/
    if (outputDir != NULL) {
      if (outputType == 'm')
        metaEndDocument(parser);
      (void) fclose(fp);
      if (result == NULL)
        (void) tremove(outName);
      free(outName);
    }
/*@=branchstate@*/
    XML_ParserFree(parser);
  }
  return 0;
}
