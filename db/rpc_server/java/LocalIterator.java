/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: LocalIterator.java,v 1.3 2003/01/08 05:45:38 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import java.util.*;

/**
 * Iterator interface.  Note that this matches java.util.Iterator
 * but maintains compatibility with Java 1.1
 * Intentionally package-protected exposure.
 */
interface LocalIterator {
	boolean hasNext();
	Object next();
	void remove();
}
