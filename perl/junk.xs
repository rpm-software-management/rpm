/*
 * This file includes misc functions that were once implemented and retired
 * later in the process
 *
 * $Id: junk.xs,v 1.1 1999/07/14 16:52:52 jbj Exp $
 */

SV *
HeaderRawItem(header, item)
  Header *	header
  int		item
  PREINIT:
	int_32 count, type;
	int rc;
	void * value;
	AV * array;
	char ** src;
  CODE:
	rc = headerGetEntry(*header, item, &type, &value, &count);
	RETVAL = &PL_sv_undef;
	if (rc != 0) {
	  switch(type) {
	    case RPM_CHAR_TYPE:
	        RETVAL = newSViv((char) (int)value);
	        break;
	    case RPM_INT8_TYPE:
	        RETVAL = newSViv((int_8) (int) value);
	        break;
	    case RPM_INT16_TYPE:
	        RETVAL = newSViv((int_16) (int) value);
	        break;
	    case RPM_INT32_TYPE:
	        RETVAL = newSViv((int_32)value);
	        break;
	    case RPM_STRING_TYPE:
	        RETVAL = newSVpv((char *)value, 0);
   	        break;
	    case RPM_BIN_TYPE:
	        /* XXX: this looks mostly unused - how do we deal with it? */
	        break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		/* we have to build an array first */
		array = newAV();
		src = (char **) value;
		while (count--) {
		  av_push(array, newSVpv(*src++, 0));
		}
		RETVAL = newRV_inc((SV*)array);
	        free(value);
	        break;
	  }
	}
  OUTPUT:
	RETVAL
