/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: Util.java,v 1.5 2004/09/23 23:56:43 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import java.io.FileNotFoundException;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;

/**
 * Helper methods for JDB <-> DB mapping
 */
public class Util {
    static int handleException(Throwable t) {
        int ret = Server.EINVAL;

        if (t instanceof DatabaseException)
            ret = ((DatabaseException)t).getErrno();
        else if (t instanceof FileNotFoundException)
            ret = Server.ENOENT;

        t.printStackTrace(Server.err);
        Server.err.println("handleException(" + t + ") returned " + ret);
        return ret;
    }

    static int notSupported(String meth) {
        Server.err.println("Unsupported functionality with JE: " + meth);
        return Server.EINVAL;
    }

    static int ignored(String meth) {
        Server.err.println("Warning functionality ignored with JE: " + meth);
        return 0;
    }

    static DatabaseEntry makeDatabaseEntry(byte[] data, int dlen, int doff, int ulen, int flags, int multiFlags) throws DatabaseException {
        DatabaseEntry dbt;
        switch (multiFlags) {
        case DbConstants.DB_MULTIPLE:
            dbt = new MultipleDataEntry(new byte[ulen]);
            break;
        case DbConstants.DB_MULTIPLE_KEY:
            dbt = new MultipleKeyDataEntry(new byte[ulen]);
            break;
        default:
            dbt = new DatabaseEntry(data);
            break;
        }
        dbt.setPartial(doff, dlen, (flags & DbConstants.DB_DBT_PARTIAL) != 0);
        return dbt;
    }

    static DatabaseEntry makeDatabaseEntry(byte[] data, int dlen, int doff, int ulen, int flags) throws DatabaseException {
        return makeDatabaseEntry(data, dlen, doff, ulen, flags, 0);
    }

    static byte[] returnDatabaseEntry(DatabaseEntry dbt) throws DatabaseException {
        if (dbt.getData().length == dbt.getSize())
            return dbt.getData();
        else {
            byte[] newdata = new byte[dbt.getSize()];
            System.arraycopy(dbt.getData(), 0, newdata, 0, dbt.getSize());
            return newdata;
        }
    }

    private static final String separator = ":::";

    static String makeFileName(String file, String database) {
        return null;
    }

    static String makeDatabaseName(String file, String database) {
        if (file == null && database == null)
            return null;
        else if (database.length() == 0 && file.indexOf(separator) >= 0)
            return file;
        return file + separator + database;
    }

    static String makeRenameTarget(String file, String database, String newname) {
        if (database.length() == 0)
            return makeDatabaseName(newname, database);
        else
            return makeDatabaseName(file, newname);
    }

    static String getFileName(String fullname) {
        if (fullname == null)
            return null;
        int pos = fullname.indexOf(separator);
        return fullname.substring(0, pos);
    }

    static String getDatabaseName(String fullname) {
        if (fullname == null)
            return null;
        int pos = fullname.indexOf(separator);
        return fullname.substring(pos + separator.length());
    }

    static LockMode getLockMode(int flags) {
        switch(flags & Server.DB_MODIFIER_MASK) {
        case DbConstants.DB_DIRTY_READ:
            return LockMode.DIRTY_READ;
        case DbConstants.DB_DEGREE_2:
            return LockMode.DEGREE_2;
        case DbConstants.DB_RMW:
            return LockMode.RMW;
        default:
            return LockMode.DEFAULT;
        }
    }

    static int getStatus(OperationStatus status) {
        if (status == OperationStatus.SUCCESS)
            return 0;
        else if (status == OperationStatus.KEYEXIST)
            return DbConstants.DB_KEYEXIST;
        else if (status == OperationStatus.KEYEMPTY)
            return DbConstants.DB_KEYEMPTY;
        else if (status == OperationStatus.NOTFOUND)
            return DbConstants.DB_NOTFOUND;
        else
            throw new IllegalArgumentException("Unknown status: " + status);
    }

    static int fromDatabaseType(DatabaseType type) {
        if (type == DatabaseType.BTREE)
            return DbConstants.DB_BTREE;
        else if (type == DatabaseType.HASH)
            return DbConstants.DB_HASH;
        else if (type == DatabaseType.QUEUE)
            return DbConstants.DB_QUEUE;
        else if (type == DatabaseType.RECNO)
            return DbConstants.DB_RECNO;
        else
            throw new
                IllegalArgumentException("Unknown database type: " + type);
    }

    static DatabaseType toDatabaseType(int type) {
        switch (type) {
        case DbConstants.DB_BTREE:
            return DatabaseType.BTREE;
        case DbConstants.DB_HASH:
            return DatabaseType.HASH;
        case DbConstants.DB_QUEUE:
            return DatabaseType.QUEUE;
        case DbConstants.DB_RECNO:
            return DatabaseType.RECNO;
        case DbConstants.DB_UNKNOWN:
            return DatabaseType.UNKNOWN;
        default:
            throw new IllegalArgumentException("Unknown database type ID: " + type);
        }
    }

    // Utility classes should not have a public or default constructor
    protected Util() {
    }
}
