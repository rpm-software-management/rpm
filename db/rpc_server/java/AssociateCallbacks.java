/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: AssociateCallbacks.java,v 1.3 2004/04/06 20:43:41 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;

/** Implementations of the callbacks required by the Tcl test suite. **/
class AssociateCallbacks {
    /*
     * Tcl passes one of these special flags for the callbacks used in the
     * test suite.  Note: these must match db_int.in!
     */
    static final int DB_RPC2ND_REVERSEDATA = 0x00100000;
    static final int DB_RPC2ND_NOOP = 0x00200000;
    static final int DB_RPC2ND_CONCATKEYDATA = 0x00300000;
    static final int DB_RPC2ND_CONCATDATAKEY = 0x00400000;
    static final int DB_RPC2ND_REVERSECONCAT = 0x00500000;
    static final int DB_RPC2ND_TRUNCDATA = 0x00600000;
    static final int DB_RPC2ND_CONSTANT = 0x00700000;
    static final int DB_RPC2ND_GETZIP = 0x00800000;
    static final int DB_RPC2ND_GETNAME = 0x00900000;

    static final int DB_RPC2ND_MASK = 0x00f00000;

    static SecondaryKeyCreator getCallback(int flags) {
        switch(flags & DB_RPC2ND_MASK) {
        case 0:
            return null;

        case DB_RPC2ND_REVERSEDATA:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    byte[] dataBuf = data.getData();
                    int dataSize = data.getSize();
                    byte[] buf = new byte[dataSize];
                    for (int i = 0; i < dataSize; i++)
                        buf[dataSize - 1 - i] = dataBuf[i];
                    result.setData(buf);
                    result.setSize(buf.length);
                    return true;
                }
            };

        case DB_RPC2ND_NOOP:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    result.setData(data.getData());
                    result.setSize(data.getSize());
                    return true;
                }
            };

        case DB_RPC2ND_CONCATKEYDATA:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    byte[] buf = new byte[key.getSize() +
                                          data.getSize()];
                    System.arraycopy(key.getData(), 0,
                                     buf, 0,
                                     key.getSize());
                    System.arraycopy(data.getData(), 0,
                                     buf, key.getSize(),
                                     data.getSize());
                    result.setData(buf);
                    result.setSize(buf.length);
                    return true;
                }
            };

        case DB_RPC2ND_CONCATDATAKEY:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    byte[] buf = new byte[key.getSize() +
                                          data.getSize()];
                    System.arraycopy(data.getData(), 0,
                                     buf, 0,
                                     data.getSize());
                    System.arraycopy(key.getData(), 0,
                                     buf, data.getSize(),
                                     key.getSize());
                    result.setData(buf);
                    result.setSize(buf.length);
                    return true;
                }
            };

        case DB_RPC2ND_REVERSECONCAT:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    byte[] keyBuf = key.getData();
                    int keySize = key.getSize();
                    byte[] dataBuf = data.getData();
                    int dataSize = data.getSize();
                    byte[] buf = new byte[keySize + dataSize];
                    for (int i = 0; i < keySize; i++)
                        buf[buf.length - 1 - i] = keyBuf[i];
                    for (int i = 0; i < dataSize; i++)
                        buf[dataSize - 1 - i] = dataBuf[i];
                    result.setData(buf);
                    result.setSize(buf.length);
                    return true;
                }
            };

        case DB_RPC2ND_TRUNCDATA:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    result.setData(data.getData());
                    result.setOffset(1);
                    result.setSize(data.getSize() - 1);
                    return true;
                }
            };

        case DB_RPC2ND_CONSTANT:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    byte[] buf = "constant data".getBytes();
                    result.setData(buf);
                    result.setSize(buf.length);
                    return true;
                }
            };

        case DB_RPC2ND_GETZIP:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    result.setData(data.getData());
                    result.setSize(5);
                    return true;
                }
            };

        case DB_RPC2ND_GETNAME:
            return new SecondaryKeyCreator() {
                public boolean createSecondaryKey(SecondaryDatabase secondary,
                                                  DatabaseEntry key, DatabaseEntry data, DatabaseEntry result)
                    throws DatabaseException {
                    result.setData(data.getData());
                    result.setOffset(5);
                    result.setSize(data.getSize() - 5);
                    return true;
                }
            };

        default:
            Server.err.println("Warning: Java RPC server doesn't implement callback: " + (flags & DB_RPC2ND_MASK));
            return null;
        }
    }

    // Utility classes should not have a public or default constructor
    protected AssociateCallbacks() {
    }
}
