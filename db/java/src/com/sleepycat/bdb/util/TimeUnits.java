/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: TimeUnits.java,v 1.1 2003/12/15 21:44:12 jbj Exp $
 */

package com.sleepycat.bdb.util;

import java.util.Calendar;
import java.util.Date;

/**
 * Common time unit definitions.
 *
 * @author Mark Hayes
 */
public class TimeUnits {

    /** One second in milliseconds. */
    public static final int  ONE_SECOND = 1000;
    /** One minute in milliseconds. */
    public static final int  ONE_MINUTE = 60 * ONE_SECOND;
    /** One hour in milliseconds. */
    public static final int  ONE_HOUR   = 60 * ONE_MINUTE;
    /** One day in milliseconds. */
    public static final long ONE_DAY    = 24 * ONE_HOUR;
    /** One week in milliseconds. */
    public static final long ONE_WEEK   = 7 * ONE_DAY;
}
