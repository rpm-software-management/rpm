/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: BindingSpeedTest.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.bind.test;

import com.sleepycat.bdb.bind.DataType;
import com.sleepycat.bdb.bind.serial.SerialInput;
import com.sleepycat.bdb.bind.serial.SerialOutput;
import com.sleepycat.bdb.bind.serial.test.TestClassCatalog;
import com.sleepycat.bdb.bind.tuple.TupleInput;
import com.sleepycat.bdb.bind.tuple.TupleOutput;
import com.sleepycat.bdb.util.FastInputStream;
import com.sleepycat.bdb.util.FastOutputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStreamWriter;
import java.io.Reader;
import java.io.Serializable;
import java.io.Writer;
import javax.xml.parsers.SAXParserFactory;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.xml.sax.InputSource;
import org.xml.sax.XMLReader;

/**
 * @author Mark Hayes
 */
public class BindingSpeedTest extends TestCase {

    static final String JAVA_UNSHARED = "java-unshared".intern();
    static final String JAVA_SHARED = "java-shared".intern();
    static final String XML_SAX = "xml-sax".intern();
    static final String TUPLE = "tuple".intern();

    static final int RUN_COUNT = 1000;
    static final boolean VERBOSE = false;

    public static void main(String[] args)
        throws Exception {

        junit.textui.TestRunner.run (suite());
    }

    public static Test suite() {

        TestSuite suite = new TestSuite();
        suite.addTest(new BindingSpeedTest(JAVA_UNSHARED));
        suite.addTest(new BindingSpeedTest(JAVA_SHARED));
        suite.addTest(new BindingSpeedTest(XML_SAX));
        suite.addTest(new BindingSpeedTest(TUPLE));
        return suite;
    }

    private String command;
    private FastOutputStream fo;
    private TupleOutput to;
    private TestClassCatalog jtc;
    private byte[] buf;
    private XMLReader parser;

    public BindingSpeedTest(String name) {

        super("BindingSpeedTest." + name);
        command = name;
    }

    public void runTest()
        throws Exception {

        System.out.println(getName());

        boolean isTuple = false;
        boolean isXmlSax = false;
        boolean isSerial = false;
        boolean isShared = false;
        boolean isFixed = false;

        int expectSize;

        if (command == TUPLE) {
            isTuple = true;
            expectSize = 29;
        } else if (command == XML_SAX) {
            isXmlSax = true;
            expectSize = 122;
        } else if (command == JAVA_UNSHARED) {
            isSerial = true;
            expectSize = 181;
        } else if (command == JAVA_SHARED) {
            isSerial = true;
            isShared = true;
            expectSize = 41;
        } else {
            throw new Exception("invalid command: " + command);
        }

        // Do initialization

        if (isTuple) {
            initTuple();
        } else if (isXmlSax) {
            initXmlSax();
        } else if (isSerial) {
            if (isShared) {
                initSerialShared();
            } else {
                initSerialUnshared();
            }
        }

        // Prime the Java compiler

        int size = 0;
        for (int i = 0; i < RUN_COUNT; i += 1) {

            if (isTuple) {
                size = runTuple();
            } else if (isXmlSax) {
                size = runXmlSax();
            } else if (isSerial) {
                if (isShared) {
                    size = runSerialShared();
                } else {
                    size = runSerialUnshared();
                }
            }
        }

        // Then run the timing tests

        long startTime = System.currentTimeMillis();

        for (int i = 0; i < RUN_COUNT; i += 1) {
            if (isTuple) {
                size = runTuple();
            } else if (isXmlSax) {
                size = runXmlSax();
            } else if (isSerial) {
                if (isShared) {
                    size = runSerialShared();
                } else {
                    size = runSerialUnshared();
                }
            }
        }

        long stopTime = System.currentTimeMillis();

        assertEquals("data size not expected", expectSize, size);

        if (VERBOSE) {
            System.out.println(command);
            System.out.println("data size: " + size);
            System.out.println("run time:  " +
                ((stopTime - startTime) / (double) RUN_COUNT));
        }
    }

    void initSerialUnshared()
        throws Exception {

        fo = new FastOutputStream();
    }

    int runSerialUnshared()
        throws Exception {

        fo.reset();
        ObjectOutputStream oos = new ObjectOutputStream(fo);
        oos.writeObject(new Data());
        byte[] bytes = fo.toByteArray();
        FastInputStream fi = new FastInputStream(bytes);
        ObjectInputStream ois = new ObjectInputStream(fi);
        ois.readObject();
        return bytes.length;
    }

    void initSerialShared()
        throws Exception {

        jtc = new TestClassCatalog();
        fo = new FastOutputStream();
    }

    int runSerialShared()
        throws Exception {

        fo.reset();
        SerialOutput oos = new SerialOutput(fo, jtc);
        oos.writeObject(new Data());
        byte[] bytes = fo.toByteArray();
        FastInputStream fi = new FastInputStream(bytes);
        SerialInput ois = new SerialInput(fi, jtc);
        ois.readObject();
        return (bytes.length - SerialOutput.getStreamHeader().length);
    }

    void initTuple()
        throws Exception {

        buf = new byte[500];
        to = new TupleOutput(buf);
    }

    int runTuple()
        throws Exception {

        to.reset();
        new Data().writeTuple(to);

        TupleInput ti = new TupleInput(
                          to.getBufferBytes(), to.getBufferOffset(),
                          to.getBufferLength());
        new Data().readTuple(ti);

        return to.getBufferLength();
    }

    void initXmlSax()
        throws Exception {

        buf = new byte[500];
        fo = new FastOutputStream();
        SAXParserFactory saxFactory = SAXParserFactory.newInstance();
        saxFactory.setNamespaceAware(true);
        parser = saxFactory.newSAXParser().getXMLReader();
    }

    int runXmlSax()
        throws Exception {

        fo.reset();
        OutputStreamWriter writer = new OutputStreamWriter(fo);
        new Data().writeXmlText(writer);

        byte[] bytes = fo.toByteArray();
        FastInputStream fi = new FastInputStream(bytes);
        InputSource input = new InputSource(fi);
        parser.parse(input);

        //InputStreamReader reader = new InputStreamReader(fi);
        //new Data().readXmlText(??);

        return bytes.length;
    }

    static class Data implements Serializable {

        String field1 = "field1";
        String field2 = "field2";
        int field3 = 333;
        int field4 = 444;
        String field5 = "field5";

        void readTuple(TupleInput _input)
            throws IOException {

            field1 = _input.readString();
            field2 = _input.readString();
            field3 = _input.readInt();
            field4 = _input.readInt();
            field5 = _input.readString();
        }

        void writeTuple(TupleOutput _output)
            throws IOException {

            _output.writeString(field1);
            _output.writeString(field2);
            _output.writeInt(field3);
            _output.writeInt(field4);
            _output.writeString(field5);
        }

        void writeXmlText(Writer writer)
            throws IOException {

            writer.write("<Data><Field1>");
            writer.write(field1);
            writer.write("</Field1><Field2>");
            writer.write(field2);
            writer.write("</Field2><Field3>");
            writer.write(String.valueOf(field3));
            writer.write("</Field3><Field4>");
            writer.write(String.valueOf(field4));
            writer.write("</Field4><Field5>");
            writer.write(field5);
            writer.write("</Field5></Data>");
            writer.flush();
        }
    }
}
