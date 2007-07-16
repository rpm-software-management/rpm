/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: RepConfigInfo.h,v 1.2 2006/08/24 14:45:50 bostic Exp $
 */
#include <db_cxx.h>

// Chainable struct used to store host information.
typedef struct RepHostInfoObj{
	char* host;
	int port;
	bool peer; // only relevant for "other" hosts
	RepHostInfoObj* next; // used for chaining multiple "other" hosts.
} REP_HOST_INFO;

class RepConfigInfo {
public:
	RepConfigInfo();
	virtual ~RepConfigInfo();

	void addOtherHost(char* host, int port, bool peer);
public:
	u_int32_t start_policy;
	char* home;
	bool got_listen_address;
	REP_HOST_INFO this_host;
	int totalsites;
	int priority;
	bool verbose;
	// used to store a set of optional other hosts.
	REP_HOST_INFO *other_hosts;
};

