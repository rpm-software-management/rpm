#ifndef H_RPMLIB
#define H_RPMLIB

/* This is the *only* modules users of rpmlib should need to include */

/* these tags are for both the database and packages */

#define RPMTAG_NAME  			1000
#define RPMTAG_VERSION			1001
#define RPMTAG_RELEASE			1002
#define RPMTAG_SERIAL   		1003
#define	RPMTAG_SUMMARY			1004
#define RPMTAG_DESCRIPTION		1005
#define RPMTAG_BUILDTIME		1006
#define RPMTAG_BUILDHOST		1007
#define RPMTAG_INSTALLTIME		1008
#define RPMTAG_SIZE			1009
#define RPMTAG_DISTRIBUTION		1010
#define RPMTAG_VENDOR			1011
#define RPMTAG_GIF			1012
#define RPMTAG_XPM			1013

#endif
