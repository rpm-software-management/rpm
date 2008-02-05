/** \ingroup header
 * \file rpmdb/header_internal.c
 */

#include "system.h"

#include <rpm/rpmtag.h>
#include "rpmdb/header_internal.h"

#include "debug.h"

char ** headerGetLangs(Header h)
{
    char **s, *e, **table;
    rpmTagType type;
    rpm_count_t i, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (rpm_data_t)&s, &count))
	return NULL;

    /* XXX xcalloc never returns NULL. */
    if ((table = (char **)xcalloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count; i++, e += strlen(e)+1)
	table[i] = e;
    table[count] = NULL;

    return table;	/* LCL: double indirection? */
}

/* FIX: shrug */
void headerDump(Header h, FILE *f, int flags,
	const struct headerTagTableEntry_s * tags)
{
    int i;
    indexEntry p;
    const struct headerTagTableEntry_s * tage;
    const char * tag;
    const char * type;

    /* First write out the length of the index (count of index entries) */
    fprintf(f, "Entry count: %d\n", h->indexUsed);

    /* Now write the index */
    p = h->index;
    fprintf(f, "\n             CT  TAG                  TYPE         "
		"      OFSET      COUNT\n");
    for (i = 0; i < h->indexUsed; i++) {
	switch (p->info.type) {
	case RPM_NULL_TYPE:
	    type = "NULL";
	    break;
	case RPM_CHAR_TYPE:
	    type = "CHAR";
	    break;
	case RPM_BIN_TYPE:
	    type = "BIN";
	    break;
	case RPM_INT8_TYPE:
	    type = "INT8";
	    break;
	case RPM_INT16_TYPE:
	    type = "INT16";
	    break;
	case RPM_INT32_TYPE:
	    type = "INT32";
	    break;
	/*case RPM_INT64_TYPE:  	type = "INT64"; 	break;*/
	case RPM_STRING_TYPE:
	    type = "STRING";
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    type = "STRING_ARRAY";
	    break;
	case RPM_I18NSTRING_TYPE:
	    type = "I18N_STRING";
	    break;
	default:
	    type = "(unknown)";
	    break;
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
			    (unsigned) *((int32_t *) dp),
			    (int) *((int32_t *) dp));
		    dp += sizeof(int32_t);
		}
		break;

	    case RPM_INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%04x (%d)\n", ct++,
			    (unsigned) (*((int16_t *) dp) & 0xffff),
			    (int) *((int16_t *) dp));
		    dp += sizeof(int16_t);
		}
		break;
	    case RPM_INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%02x (%d)\n", ct++,
			    (unsigned) (*((int8_t *) dp) & 0xff),
			    (int) *((int8_t *) dp));
		    dp += sizeof(int8_t);
		}
		break;
	    case RPM_BIN_TYPE:
		while (c > 0) {
		    fprintf(f, "       Data: %.3d ", ct);
		    while (c--) {
			fprintf(f, "%02x ", (unsigned) (*(int8_t *)dp & 0xff));
			ct++;
			dp += sizeof(int8_t);
			if (! (ct % 8)) {
			    break;
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

char * bin2hex(const char *data, size_t size)
{
    static char hex[] = "0123456789abcdef";
    const char * s = data;
    char * t, * val;
    val = t = xmalloc(size * 2 + 1);
    while (size-- > 0) {
	unsigned int i;
	i = *s++;
	*t++ = hex[ (i >> 4) & 0xf ];
	*t++ = hex[ (i     ) & 0xf ];
    }
    *t = '\0';

    return val;
}
    
    
