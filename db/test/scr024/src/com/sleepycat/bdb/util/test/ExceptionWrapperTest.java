/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ExceptionWrapperTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.util.test;

import com.sleepycat.bdb.util.ExceptionUnwrapper;
import com.sleepycat.bdb.util.IOExceptionWrapper;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.io.IOException;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * @author Mark Hayes
 */
public class ExceptionWrapperTest extends TestCase {

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run(suite());
    }

    public static Test suite()
        throws Exception {

        TestSuite suite = new TestSuite(ExceptionWrapperTest.class);
        return suite;
    }

    public ExceptionWrapperTest(String name) {

        super(name);
    }

    public void setUp() {

        System.out.println("ExceptionWrapperTest." + getName());
    }

    public void testIOWrapper()
        throws Exception {

        try {
            throw new IOExceptionWrapper(new RuntimeException("msg"));
        } catch (IOException e) {
            Exception ee = ExceptionUnwrapper.unwrap(e);
            assertTrue(ee instanceof RuntimeException);
            assertEquals("msg", ee.getMessage());

            Throwable t = ExceptionUnwrapper.unwrapAny(e);
            assertTrue(t instanceof RuntimeException);
            assertEquals("msg", t.getMessage());
        }
    }

    public void testRuntimeWrapper()
        throws Exception {

        try {
            throw new RuntimeExceptionWrapper(new IOException("msg"));
        } catch (RuntimeException e) {
            Exception ee = ExceptionUnwrapper.unwrap(e);
            assertTrue(ee instanceof IOException);
            assertEquals("msg", ee.getMessage());

            Throwable t = ExceptionUnwrapper.unwrapAny(e);
            assertTrue(t instanceof IOException);
            assertEquals("msg", t.getMessage());
        }
    }

    public void testErrorWrapper()
        throws Exception {

        try {
            throw new RuntimeExceptionWrapper(new Error("msg"));
        } catch (RuntimeException e) {
            try {
                ExceptionUnwrapper.unwrap(e);
                fail();
            } catch (Error ee) {
                assertTrue(ee instanceof Error);
                assertEquals("msg", ee.getMessage());
            }

            Throwable t = ExceptionUnwrapper.unwrapAny(e);
            assertTrue(t instanceof Error);
            assertEquals("msg", t.getMessage());
        }
    }
}

