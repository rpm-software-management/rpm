#include <stdio.h>
#include <string.h>
#include "rpmchk.h"
#include "tagfuncs.h"
#include "../tetj/tetj.h"

void
checkRpmHdr(RpmFile *file1, struct tetj_handle *journal)
{
#define TMP_STRING_SIZE (400)
char tmp_string[TMP_STRING_SIZE+1];
RpmHeader	*hdr;
RpmHdrIndex	*hindex;

hdr=(RpmHeader *)file1->nexthdr;
hindex=(RpmHdrIndex *)(hdr+1);

/*
fprintf(stderr,"checkRpmHdr() hdr=%x\n", hdr );
*/

/* Check the RpmHeader magic value */
tetj_tp_count++;
tetj_purpose_start(journal, tetj_activity_count, tetj_tp_count, "Check magic value");
if(strncmp(hdr->magic,RPMHDRMAG,SRPMHDRMAG)) {
        snprintf( tmp_string, TMP_STRING_SIZE, "compareRpmHeader: magic isn't expected value RPMHDRMAG, found %x %x %x instead\n", hdr->magic[0], hdr->magic[1], hdr->magic[2]); 
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count, 0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
} else {
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_PASS); 
}
tetj_purpose_end(journal, tetj_activity_count, tetj_tp_count); 

/* Check the RpmHeader version */
tetj_tp_count++;
tetj_purpose_start(journal, tetj_activity_count, tetj_tp_count, "Check magic value");
if(hdr->version != RPMHDRVER ) {
        snprintf( tmp_string, TMP_STRING_SIZE, "compareRpmHeader: magic isn't expected value RPMHDRMAG, found %x %x %x instead\n", hdr->magic[0], hdr->magic[1], hdr->magic[2]); 
        fprintf(stderr, "%s\n", tmp_string);
        tetj_testcase_info(journal, tetj_activity_count, tetj_tp_count, 0, 0, 0, tmp_string);
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_FAIL);
} else {
        tetj_result(journal, tetj_activity_count, tetj_tp_count, TETJ_PASS); 
}
tetj_purpose_end(journal, tetj_activity_count, tetj_tp_count); 

}

void
checkRpmSignature(RpmFile *file1, struct tetj_handle *journal)
{
RpmHeader	*hdr;
RpmHdrIndex	*hindex;
int	nindex;

hdr=(RpmHeader *)file1->nexthdr;
hindex=(RpmHdrIndex *)(hdr+1);
nindex=ntohl(hdr->nindex);
file1->storeaddr=(((char *)hdr)+sizeof(RpmHeader)+(nindex*sizeof(RpmHdrIndex)));
file1->header=(RpmHeader *)((char *)file1->storeaddr+
		ntohl(hindex->offset)+ntohl(hindex->count));

/*
fprintf(stderr,"Signature has %d indicies with %x bytes of store at %x\n",
			nindex, ntohl(hdr->hsize),file1->storeaddr);
*/

checkRpmHdr(file1, journal);
checkRpmIdx(file1, hindex, SigTags, numSigIdxTags, journal);

file1->nexthdr=(RpmHeader *)((char *)file1->storeaddr+
		ntohl(hindex->offset)+ntohl(hindex->count));
}

void
checkRpmHeader(RpmFile *file1, struct tetj_handle *journal)
{
RpmHeader	*hdr;
RpmHdrIndex	*hindex;
int	nindex;

hdr=(RpmHeader *)file1->nexthdr;
hindex=(RpmHdrIndex *)(hdr+1);
nindex=ntohl(hdr->nindex);
file1->storeaddr=(((char *)hdr)+sizeof(RpmHeader)+(nindex*sizeof(RpmHdrIndex)));

/*
fprintf(stderr,"Header has %d indicies with %x bytes of store at %x\n",
			nindex, ntohl(hdr->hsize),file1->storeaddr);
*/

checkRpmHdr(file1, journal);
checkRpmIdx(file1, hindex, HdrTags, numHdrIdxTags, journal);

file1->nexthdr=(RpmHeader *)((char *)file1->storeaddr+
		ntohl(hindex->offset)+ntohl(hindex->count));
}
