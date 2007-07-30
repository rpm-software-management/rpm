#ifndef HDBREC_H
#define	HDBREC_H

#include "htimestampxa.h"

/*
 * DB record
 */
typedef struct __HDbRec {
	long SeqNo;
	HTimestampData Ts;
	char Msg[10];
} HDbRec;
#endif
