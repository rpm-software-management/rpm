#include <stdio.h>
#include <string.h>
#include "rpmchk.h"
#include "../tetj/tetj.h"

void
checkRpmLead(RpmFile *file1, struct tetj_handle *journal)
{
#define TMP_STRING_SIZE (400)
char tmp_string[TMP_STRING_SIZE+1];
RpmLead	*rlead;

rlead=(RpmLead *)file1->addr;
file1->laddr=rlead;

tetj_tp_count++;
tetj_purpose_start(journal, tetj_activity_count, tetj_tp_count, "Check magic value");
if(strncmp(rlead->magic,RPMMAG,SRPMMAG)) {
        snprintf( tmp_string, TMP_STRING_SIZE, "compareRpmLead: magic isn't expected value RPMMAG, found %x %x %x %x instead\n", rlead->magic[0], rlead->magic[1], rlead->magic[2], rlead->magic[3]); 
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count, 0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
} else {
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_PASS); 
}
tetj_purpose_end(journal, tetj_activity_count, tetj_tp_count); 

#define checkhdrfield( member, value ) \
{ \
tetj_tp_count++; \
tetj_purpose_start(journal, tetj_activity_count, tetj_tp_count, "Check header field "#member" is "#value); \
if( rlead->member != value ) { \
        snprintf( tmp_string, TMP_STRING_SIZE, "compareRpmLead: %s isn't expected value %s, found %x instead\n", #member, #value, rlead->member); \
        fprintf(stderr, "%s\n", tmp_string); \
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count, 0, 0, 0, tmp_string); \
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL); \
        } \
else \
{ \
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_PASS); \
} \
tetj_purpose_end(journal, tetj_activity_count, tetj_tp_count); \
        }

checkhdrfield( major, RPMFILEVERMAJ )
checkhdrfield( minor, RPMFILEVERMIN )

checkhdrfield( type, htons(RPMBINPKG) )

checkhdrfield( archnum, htons(RPMARCH) )

checkhdrfield( osnum, htons(RPMOS) )

checkhdrfield( signature_type, htons(RPMSIGTYPE) )
}
