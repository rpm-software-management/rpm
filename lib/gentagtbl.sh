#!/bin/sh

cat << EOF
#include "system.h"
#include <rpm/header.h>
#include <rpm/rpmtag.h>
#include "debug.h"

static const struct headerTagTableEntry_s rpmTagTbl[] = {
EOF

${AWK} '/[\t ](RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ {
	tt = "NULL"
	ta = "ANY"
	if ($5 == "c") {
		tt = "CHAR"
		ta = "SCALAR"
	}
	if ($5 == "c[]") {
		tt = "CHAR"
		ta = "ARRAY"
	} 
	if ($5 == "h") {
		tt = "INT16"
		ta = "SCALAR"
	}
	if ($5 == "h[]") {
		tt = "INT16"
		ta = "ARRAY"
	}
	if ($5 == "i") {
		tt = "INT32"
		ta = "SCALAR"
	}
	if ($5 == "i[]") {
		tt = "INT32"
		ta = "ARRAY"
	}
	if ($5 == "l") {
		tt = "INT64"
		ta = "SCALAR"
	}
	if ($5 == "l[]") {
		tt = "INT64"
		ta = "ARRAY"
	}
	if ($5 == "s") {
		tt = "STRING"
		ta = "SCALAR"
	}
	if ($5 == "s[]") {
		tt = "STRING_ARRAY"
		ta = "ARRAY"
	}
	if ($5 == "s{}") {
		tt = "I18NSTRING"
		ta = "SCALAR"
	} 
	if ($5 == "x") {
		tt = "BIN"
		ta = "SCALAR"
	}
	if ($2 == "=") {
		tnarg = $1
	} else {
		tnarg = $2
	}
	tn = substr(tnarg, index(tnarg, "_") + 1)
	sn = (substr(tn, 1, 1) tolower(substr(tn, 2)))
	if ($2 == "=") {
		printf("    { \"%s\", \"%s\", %s RPM_%s_TYPE + RPM_%s_RETURN_TYPE  },\n", tnarg, sn, $3, tt, ta)
	} else {
		printf("    { \"%s\", \"%s\", %s, RPM_%s_TYPE + RPM_%s_RETURN_TYPE  },\n", tnarg, sn, $3, tt, ta)
	}
}' < $1 | sort

cat << EOF
    { NULL, NULL, 0, 0 }
};

const struct headerTagTableEntry_s * const rpmTagTable = rpmTagTbl;
const int rpmTagTableSize = sizeof(rpmTagTbl) / sizeof(rpmTagTbl[0]) - 1;
EOF

