/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: SecondaryMultiKeyCreator.java,v 12.3 2007/06/28 14:23:36 mjc Exp $
 */

package com.sleepycat.db;

import java.util.Set;

/**
 * Javadoc for this public method is generated via
 * the doc templates in the doc_src directory.
 */
public interface SecondaryMultiKeyCreator {
    /**
     * Javadoc for this public method is generated via
     * the doc templates in the doc_src directory.
     */
    public void createSecondaryKeys(SecondaryDatabase secondary,
                                    DatabaseEntry key,
                                    DatabaseEntry data,
                                    Set results)
        throws DatabaseException;
}
