/*
 * Perl interface to rpmlib
 *
 * $Id: rpm.xs,v 1.1 1999/07/14 16:52:52 jbj Exp $
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <rpm/rpmio.h>
#include <rpm/dbindex.h>
#include <rpm/header.h>
#include <popt.h>
#include <rpm/rpmlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern double constant(char *name, int arg);

MODULE = rpm		PACKAGE = rpm		

PROTOTYPES: ENABLE

double
constant(name,arg)
	char *		name
	int		arg

Header *
Header(package)
  const char *	package
  PREINIT:
	FD_t file_desc = NULL;
	int rc;
	int isSource;
	int had_error = 0;
  CODE:
	New(1,RETVAL,1,Header);
	file_desc = fdOpen(package, O_RDONLY, 0);
	if (file_desc != NULL && RETVAL != NULL) {
	  rc = rpmReadPackageHeader(file_desc, RETVAL, &isSource, NULL, NULL);
	  if (rc != 0) {
	    had_error++;
	  }
	  if (file_desc != NULL) {
	    fdClose(file_desc);
	  }
	} else {
	  had_error++;
	}
	ST(0) = sv_newmortal();
	if (had_error) {
	  ST(0) = &PL_sv_undef;
	} else {
	  sv_setref_pv(ST(0), "HeaderPtr", (void*)RETVAL);
	}


MODULE = rpm	PACKAGE = HeaderPtr	PREFIX = Header

AV *
HeaderItemByValRef(header, item)
  Header *	header
  int		item
  PREINIT:
	int_32 count, type;
	int rc;
	void * value;
	char ** src;
	AV * array;
CODE:
	rc = headerGetEntry(*header, item, &type, &value, &count);
	array = newAV();
	if (rc != 0) {
	  switch(type) {
	    case RPM_CHAR_TYPE:
	        av_push(array, newSViv((char) (int) value));
	        break;
	    case RPM_INT8_TYPE:
	        av_push(array, newSViv((int_8) (int) value));
	        break;
	    case RPM_INT16_TYPE:
	        av_push(array, newSViv((int_16) (int) value));
	        break;
	    case RPM_INT32_TYPE:
	        av_push(array, newSViv((int_32)value));
	        break;
	    case RPM_STRING_TYPE:
	        av_push(array, newSVpv((char *)value, 0));
   	        break;
	    case RPM_BIN_TYPE:
	        /* XXX: this looks mostly unused - how do we deal with it? */
	        break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		src = (char **) value;
		while (count--) {
		  av_push(array, newSVpv(*src++, 0));
		}
	        free(value);
	        break;
	  }
	}
	RETVAL = array;
  OUTPUT:
	RETVAL

AV *
HeaderItemByNameRef(header, tag)
  Header *	header
  const char *	tag
  PREINIT:
	int_32 count, type, item = -1;
	int rc, i;
	void * value;
	char ** src;
	AV * array;
  CODE:
	/* walk first through the list of items and get the proper value */
	for (i = 0; i < rpmTagTableSize; i++) {
	  if (strcasecmp(tag, rpmTagTable[i].name) == 0) {
	    item = rpmTagTable[i].val;
	    break;
	  }
	}
	rc = headerGetEntry(*header, item, &type, &value, &count);
	array = newAV();
	if (rc != 0) {
	  switch(type) {
	    case RPM_CHAR_TYPE:
	        av_push(array, newSViv((char) (int) value));
	        break;
	    case RPM_INT8_TYPE:
	        av_push(array, newSViv((int_8) (int) value));
	        break;
	    case RPM_INT16_TYPE:
	        av_push(array, newSViv((int_16) (int) value));
	        break;
	    case RPM_INT32_TYPE:
	        av_push(array, newSViv((int_32)value));
	        break;
	    case RPM_STRING_TYPE:
	        av_push(array, newSVpv((char *)value, 0));
   	        break;
	    case RPM_BIN_TYPE:
	        /* XXX: this looks mostly unused - how do we deal with it? */
	        break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		src = (char **) value;
		while (count--) {
		  av_push(array, newSVpv(*src++, 0));
		}
	        free(value);
	        break;
	  }
	}
	RETVAL = array;
  OUTPUT:
	RETVAL


HV *
HeaderListRef(header)
  Header *	header
  PREINIT:
	HeaderIterator iterator;
	int_32 tag, type, count;
	void *value;
  CODE:
	RETVAL = newHV();
	iterator = headerInitIterator(*header);
	while (headerNextIterator(iterator, &tag, &type, &value, &count)) {
	  SV  ** sv;
	  AV   * array;
          char ** src;
	  char * tagStr = tagName(tag);
	  array = newAV();
	  switch(type) {
	    case RPM_CHAR_TYPE:
		av_push(array, newSViv((char) (int) value));
	        break;
	    case RPM_INT8_TYPE:
	        av_push(array, newSViv((int_8) (int) value));
	        break;
	    case RPM_INT16_TYPE:
	        av_push(array, newSViv((int_16) (int) value));
	        break;
	    case RPM_INT32_TYPE:
	        av_push(array, newSViv((int_32)value));
	        break;
	    case RPM_STRING_TYPE:
	        av_push(array, newSVpv((char *)value, 0));
   	        break;
	    case RPM_BIN_TYPE:
	        /* XXX: this looks mostly unused - how do we deal with it? */
	        break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		/* we have to build an array first */
		src = (char **) value;
		while (count--) {
		  av_push(array, newSVpv(*src++, 0));
		}
	        free(value);
	        break;
	  }
	  sv = hv_store(RETVAL, tagStr, strlen(tagStr), newRV_inc((SV*)array), 0);
	}
	headerFreeIterator(iterator);
  OUTPUT:
	RETVAL

AV * 
HeaderTagsRef(header) 
  Header *      header 
  PREINIT: 
        HeaderIterator iterator; 
        int_32 tag, type;
        void *value; 
  CODE: 
        RETVAL = newAV(); 
	iterator = headerInitIterator(*header); 
	while (headerNextIterator(iterator, &tag, &type, &value, NULL)) { 
	  av_push(RETVAL, newSVpv(tagName(tag), 0));
	  if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    free(value);
	}
	headerFreeIterator(iterator);      
  OUTPUT:
	RETVAL
