/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2006
 *      Oracle Corporation.  All rights reserved.
 *
 * $Id: KeyRangeException.java,v 1.2 2006/08/31 18:14:11 bostic Exp $
 */

package com.sleepycat.util.keyrange;

/**
 * An exception thrown when a key is out of range.
 *
 * @author Mark Hayes
 */
public class KeyRangeException extends IllegalArgumentException {

    /**
     * Creates a key range exception.
     */
    public KeyRangeException(String msg) {

        super(msg);
    }
}
