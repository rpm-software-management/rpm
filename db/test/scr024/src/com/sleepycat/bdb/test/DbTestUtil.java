/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: DbTestUtil.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import junit.framework.TestCase;

/**
 * @author Mark Hayes
 */
public class DbTestUtil {

    public static File getNewDir()
        throws IOException {

        return getNewDir("test-dir");
    }

    public static File getNewDir(String name)
        throws IOException {

        File dir = new File("tmp", name);
        if (dir.isDirectory()) {
            String[] files = dir.list();
            if (files != null) {
                for (int i = 0; i < files.length; i += 1) {
                    new File(dir, files[i]).delete();
                }
            }
        } else {
            dir.delete();
            dir.mkdirs();
        }
        return dir;
    }

    public static File getNewFile()
        throws IOException {

        return getNewFile("test-file");
    }

    public static File getNewFile(String name)
        throws IOException {

        return getNewFile(new File("tmp"), name);
    }

    public static File getNewFile(File dir, String name)
        throws IOException {

        File file = new File(dir, name);
        file.delete();
        return file;
    }

    public static boolean copyResource(Class cls, String fileName, File toDir)
        throws IOException {

        InputStream in = cls.getResourceAsStream("testdata/" + fileName);
        if (in == null) return false;
        in = new BufferedInputStream(in);
        File file = new File(toDir, fileName);
        OutputStream out = new FileOutputStream(file);
        out = new BufferedOutputStream(out);
        int c;
        while ((c = in.read()) >= 0) out.write(c);
        in.close();
        out.close();
        return true;
    }

    public static String qualifiedTestName(TestCase test) {

        String s = test.getClass().getName();
        int i = s.lastIndexOf('.');
        if (i >= 0) s = s.substring(i + 1);
        return s + '.' + test.getName();
    }
}
