#ifndef HTIMESTAMPXA_H
#define	HTIMESTAMPXA_H

/*
 * Timestamp with microseconds precision
 */
typedef struct __HTimestampData {
	time_t Sec;
	time_t Usec;
} HTimestampData;

void GetTime(HTimestampData *);
#endif
