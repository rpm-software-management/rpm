/** \ingroup header
 * \file lib/header_internal.c
 */

#include "system.h"

#include <header_internal.h>

#include "debug.h"

char ** headerGetLangs(Header h)
{
    char **s, *e, **table;
    int i, type, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (const void **)&s, &count))
	return NULL;

    /* XXX xcalloc never returns NULL. */
    if ((table = (char **)xcalloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count > 0; i++, e += strlen(e)+1)
	table[i] = e;
    table[count] = NULL;

    /*@-nullret@*/ return table; /*@=nullret@*/	/* LCL: double indirection? */
}

void headerDump(Header h, FILE *f, int flags,
	const struct headerTagTableEntry * tags)
{
    int i;
    indexEntry p;
    const struct headerTagTableEntry * tage;
    const char * tag;
    char * type;

    /* First write out the length of the index (count of index entries) */
    fprintf(f, "Entry count: %d\n", h->indexUsed);

    /* Now write the index */
    p = h->index;
    fprintf(f, "\n             CT  TAG                  TYPE         "
		"      OFSET      COUNT\n");
    for (i = 0; i < h->indexUsed; i++) {
	switch (p->info.type) {
	case RPM_NULL_TYPE:   		type = "NULL"; 	break;
	case RPM_CHAR_TYPE:   		type = "CHAR"; 	break;
	case RPM_BIN_TYPE:   		type = "BIN"; 	break;
	case RPM_INT8_TYPE:   		type = "INT8"; 	break;
	case RPM_INT16_TYPE:  		type = "INT16"; 	break;
	case RPM_INT32_TYPE:  		type = "INT32"; 	break;
	/*case RPM_INT64_TYPE:  	type = "INT64"; 	break;*/
	case RPM_STRING_TYPE: 	    	type = "STRING"; 	break;
	case RPM_STRING_ARRAY_TYPE: 	type = "STRING_ARRAY"; break;
	case RPM_I18NSTRING_TYPE: 	type = "I18N_STRING"; break;
	default:		    	type = "(unknown)";	break;
	}

	tage = tags;
	while (tage->name && tage->val != p->info.tag) tage++;

	if (!tage->name)
	    tag = "(unknown)";
	else
	    tag = tage->name;

	fprintf(f, "Entry      : %3.3d (%d)%-14s %-18s 0x%.8x %.8d\n", i,
		p->info.tag, tag, type, (unsigned) p->info.offset,
		(int) p->info.count);

	if (flags & HEADER_DUMP_INLINE) {
	    char *dp = p->data;
	    int c = p->info.count;
	    int ct = 0;

	    /* Print the data inline */
	    switch (p->info.type) {
	    case RPM_INT32_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%08x (%d)\n", ct++,
			    (unsigned) *((int_32 *) dp),
			    (int) *((int_32 *) dp));
		    dp += sizeof(int_32);
		}
		break;

	    case RPM_INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%04x (%d)\n", ct++,
			    (unsigned) (*((int_16 *) dp) & 0xffff),
			    (int) *((int_16 *) dp));
		    dp += sizeof(int_16);
		}
		break;
	    case RPM_INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%02x (%d)\n", ct++,
			    (unsigned) (*((int_8 *) dp) & 0xff),
			    (int) *((int_8 *) dp));
		    dp += sizeof(int_8);
		}
		break;
	    case RPM_BIN_TYPE:
		while (c > 0) {
		    fprintf(f, "       Data: %.3d ", ct);
		    while (c--) {
			fprintf(f, "%02x ", (unsigned) (*(int_8 *)dp & 0xff));
			ct++;
			dp += sizeof(int_8);
			if (! (ct % 8)) {
			    /*@loopbreak@*/ break;
			}
		    }
		    fprintf(f, "\n");
		}
		break;
	    case RPM_CHAR_TYPE:
		while (c--) {
		    char ch = (char) *((char *) dp);
		    fprintf(f, "       Data: %.3d 0x%2x %c (%d)\n", ct++,
			    (unsigned)(ch & 0xff),
			    (isprint(ch) ? ch : ' '),
			    (int) *((char *) dp));
		    dp += sizeof(char);
		}
		break;
	    case RPM_STRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d %s\n", ct++, (char *) dp);
		    dp = strchr(dp, 0);
		    dp++;
		}
		break;
	    default:
		fprintf(stderr, _("Data type %d not supported\n"), 
			(int) p->info.type);
		break;
	    }
	}
	p++;
    }
}

