#include "rpmchk.h"

void
checkRpm(RpmFile *file1, struct tetj_handle *journal)
{

checkRpmLead(file1, journal);

file1->signature=(RpmHeader *)(file1->addr+sizeof(RpmLead));
file1->nexthdr=file1->signature;

/* RPM packages should have 2 Headers, the Signature, and the Header */
checkRpmSignature(file1, journal);
checkRpmHeader(file1, journal);
checkRpmArchive(file1, journal);
}
