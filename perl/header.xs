
MODULE = rpm		PACKAGE = Header		PREFIX = Header

AV *
HeaderItemByValRef(header, item)
  Header	header
  int		item
  PREINIT:
	int_32 count, type;
	int rc;
	void * value;
	char ** src;
	AV * array;
  CODE:
	rc = headerGetEntry(header, item, &type, &value, &count);
	array = newAV();
	if (rc != 0) {
	  switch(type) {
	    case RPM_CHAR_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((char) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT8_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_8) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT16_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_16) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT32_TYPE:
		while (count-- > 0) {		    
		    av_push(array, newSViv((int_32)value));
		    value++;
		}
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
  Header	header
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
	  if (rpmTagTable[i].name != NULL && strcasecmp(tag, rpmTagTable[i].name + 7) == 0) {
	    item = rpmTagTable[i].val;
	    break;
	  }
	}
	rc = headerGetEntry(header, item, &type, &value, &count);
	array = newAV();
	if (rc != 0) {
	  switch(type) {
	    case RPM_CHAR_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((char) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT8_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_8) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT16_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_16) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT32_TYPE:
		while (count-- > 0) {		    
		    av_push(array, newSViv((int_32)value));
		    value++;
		}
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
  Header	header
  PREINIT:
	HeaderIterator iterator;
	int_32 tag, type, count;
	void *value;
  CODE:
	RETVAL = newHV();
	iterator = headerInitIterator(header);
	while (headerNextIterator(iterator, &tag, &type, &value, &count)) {
	  SV  ** sv;
	  AV   * array;
          char ** src;
	  char * tagStr = tagName(tag);
	  array = newAV();
	  switch(type) {
	    case RPM_CHAR_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((char) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT8_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_8) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT16_TYPE:
		while (count-- > 0) {
		    av_push(array, newSViv((int_16) (int) value));
		    value++;
		}
	        break;
	    case RPM_INT32_TYPE:
		while (count-- > 0) {		    
		    av_push(array, newSViv((int_32)value));
		    value++;
		}
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
  Header      header 
  PREINIT: 
        HeaderIterator iterator; 
        int_32 tag, type;
        void *value; 
  CODE: 
        RETVAL = newAV(); 
	iterator = headerInitIterator(header); 
	while (headerNextIterator(iterator, &tag, &type, &value, NULL)) { 
	  av_push(RETVAL, newSVpv(tagName(tag), 0));
	  if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    free(value);
	}
	headerFreeIterator(iterator);      
  OUTPUT:
	RETVAL

