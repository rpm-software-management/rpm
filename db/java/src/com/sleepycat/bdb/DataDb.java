/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DataDb.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbException;
import com.sleepycat.db.Dbt;
import com.sleepycat.db.DbTxn;
import com.sleepycat.bdb.util.RuntimeExceptionWrapper;
import java.util.ArrayList;
import java.util.List;

/**
 * (<em>internal</em>) Wraps a Berkeley DB database (Db) object and adds
 * normalization of certain flags and environment modes.
 *
 * <p><b>NOTE:</b> This classes is internal and may be changed incompatibly or
 * deleted in the future.  It is public only so it may be used by
 * subpackages.</p>
 *
 * @author Mark Hayes
 */
public class DataDb {

    public static final int ENOMEM = 12; // from errorno.h
    public static final int EINVAL = 22; // from errorno.h

    public static final int FLAGS_POS_MASK = 0xff;            // flags position
    public static final int FLAGS_MOD_MASK = ~FLAGS_POS_MASK; // and modifiers

    DataEnvironment env;
    Db db;
    int type;
    boolean ordered;
    private ThreadLocal cdbContext;
    private String file;
    private String database;
    boolean recNumAccess;
    boolean keysRenumbered;
    boolean dupsAllowed;
    boolean dupsOrdered;
    boolean transactional;
    boolean dirtyReadAllowed;

    /**
     * Creates a database wrapper.
     *
     * @param db is the underlying database.
     */
    public DataDb(Db db) {

        try {
            this.env = DataEnvironment.getEnvironment(db.getDbEnv());
            this.file = db.getFileName();
            this.database = db.getDatabaseName();
            this.db = db;
            this.type = db.getDbType();
            this.ordered = (type == Db.DB_BTREE || type == Db.DB_QUEUE ||
                            type == Db.DB_RECNO);
            this.recNumAccess = (type != Db.DB_HASH &&
                                 (type != Db.DB_BTREE ||
                                  (db.getFlags() & Db.DB_RECNUM) != 0));
            this.keysRenumbered = (type == Db.DB_RECNO &&
                                   (db.getFlags() & Db.DB_RENUMBER) != 0);
            this.dupsOrdered = (db.getFlags() & Db.DB_DUPSORT) != 0;
            this.dupsAllowed = (this.dupsOrdered ||
                                ((db.getFlags() & Db.DB_DUP) != 0));
            this.dirtyReadAllowed = (db.getOpenFlags() & Db.DB_DIRTY_READ) != 0;
            this.transactional = env.isTxnMode() && db.isTransactional();
            if (env.isCdbMode()) {
                this.cdbContext = new ThreadLocal();
                this.cdbContext.set(new CdbThreadContext(this));
            }
        } catch (DbException e) {
            throw new RuntimeExceptionWrapper(e);
        }
    }

    /**
     * Closes the database.
     */
    public void close()
        throws DbException {

        db.close(0);
    }

    /**
     * Returns the environment.
     *
     * @return the environment.
     */
    public final DataEnvironment getEnv() {

        return env;
    }

    /**
     * Returns the underlying database.
     *
     * @return the underlying database.
     */
    public final Db getDb() {

        return db;
    }

    /**
     * Returns whether keys are ordered for the database.
     *
     * @return whether keys are ordered.
     */
    public final boolean isOrdered() {

        return ordered;
    }

    /**
     * Returns whether duplicates are allowed for the database.
     *
     * @return whether duplicates are allowed.
     */
    public final boolean areDuplicatesAllowed() {

        return dupsAllowed;
    }

    /**
     * Returns whether duplicates are ordered for the database.
     *
     * @return whether duplicates are ordered.
     */
    public final boolean areDuplicatesOrdered() {

        return dupsOrdered;
    }

    /**
     * Returns whether keys (record numbers) are renumbered for the database.
     *
     * @return whether keys are renumbered.
     */
    public final boolean areKeysRenumbered() {

        return keysRenumbered;
    }

    /**
     * Returns whether record number access is allowed.
     *
     * @return whether record number access is allowed.
     */
    public final boolean hasRecNumAccess() {

        return recNumAccess;
    }

    /**
     * Returns whether the database was opened in a transaction and therefore
     * must be written in a transaction.
     *
     * @return whether the database is transactional.
     */
    public final boolean isTransactional() {

        return transactional;
    }

    /**
     * Returns whether dirty-read is allowed for the database.
     *
     * @return whether dirty-read is allowed.
     */
    public final boolean isDirtyReadAllowed() {

        return dirtyReadAllowed;
    }

    /**
     * Performs a general database 'get' operation.
     *
     * @param key the key thang.
     *
     * @param data the data thang.
     *
     * @param flags the low-level get flags.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     */
    public int get(DataThang key, DataThang data, int flags)
        throws DbException {

        int pos = flags & DataDb.FLAGS_POS_MASK;
        if (cdbContext != null) {
            Dbc cursor = openCursor(flags == Db.DB_CONSUME ||
                                    flags == Db.DB_CONSUME_WAIT);
            try {
                if (pos == 0)
                    flags |= Db.DB_SET;
                return get(cursor, key, data, flags);
            } finally {
                closeCursor(cursor);
            }
        } else {
            if (isRecnoKeyNonPositive(pos, key))
                return Db.DB_NOTFOUND;
            return db.get(currentTxn(), key, data, flags);
        }
    }

    /**
     * Performs a general database 'get' operation via a cursor.
     *
     * @param cursor the cursor to read.
     *
     * @param key the key thang.
     *
     * @param val the data thang.
     *
     * @param flags the low-level get flags.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     */
    public int get(Dbc cursor, DataThang key, DataThang val, int flags)
        throws DbException {

        int pos = flags & DataDb.FLAGS_POS_MASK;
        if (isRecnoKeyNonPositive(pos, key))
            return Db.DB_NOTFOUND;
        int err = cursor.get(key, val, flags);
        return err;
    }

    private boolean isRecnoKeyNonPositive(int pos, DataThang key) {

        if ((pos == Db.DB_SET_RECNO) ||
            ((type == Db.DB_RECNO ||
              type == Db.DB_QUEUE) &&
             (pos == Db.DB_SET ||
              pos == Db.DB_SET_RANGE ||
              pos == Db.DB_GET_BOTH))) {
            if (key.get_recno_key_data() <= 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * Performs a general database 'put' operation.
     *
     * @param key the key to put.
     *
     * @param data the data to put.
     *
     * @param flags the low-level put flags.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     */
    public int put(DataThang key, DataThang data, int flags)
        throws DbException {

        // Dbc.put() cannot be used with QUEUE/RECNO types
        if (cdbContext != null && (type == Db.DB_HASH ||
                                   type == Db.DB_BTREE)) {
            Dbc cursor = openCursor(true);
            try {
                int pos = flags & DataDb.FLAGS_POS_MASK;
                if (pos == Db.DB_NODUPDATA && !areDuplicatesOrdered()) {
                    if (areDuplicatesAllowed()) {
                        int err = get(cursor, key, data, Db.DB_GET_BOTH);
                        if (err == 0) return Db.DB_KEYEXIST;
                        pos = 0;
                    } else {
                        pos = Db.DB_NOOVERWRITE;
                    }
                    flags = pos | (flags & DataDb.FLAGS_MOD_MASK);
                }
                return put(cursor, key, data, flags);
            } finally {
                closeCursor(cursor);
            }
        } else {
            if (cdbContext != null) { // QUEUE/RECNO
                CdbThreadContext context = (CdbThreadContext) cdbContext.get();
                if (context.writeCursors.size() > 0 ||
                    context.readCursors.size() > 0) {
                    throw new IllegalStateException(
                        "cannot put() with CDB write cursor open");
                }
            }
            int pos = flags & DataDb.FLAGS_POS_MASK;
            if (pos == Db.DB_NODUPDATA && !areDuplicatesOrdered()) {
                if (areDuplicatesAllowed()) {
                    int err = get(key, data, Db.DB_GET_BOTH);
                    if (err == 0) return Db.DB_KEYEXIST;
                    pos = 0;
                } else {
                    pos = Db.DB_NOOVERWRITE;
                }
                flags = pos | (flags & DataDb.FLAGS_MOD_MASK);
            }
            DbTxn txn = currentTxn();
            if (txn != null || !transactional)
                flags &= ~Db.DB_AUTO_COMMIT;
            int err = db.put(txn, key, data, flags);
            return err;
        }
    }

    /**
     * Performs a general database 'put' operation via a cursor.
     * This method works for HASH/BTREE types and all flags, or with all types
     * and DB_CURRENT only.
     *
     * @param cursor the cursor to write.
     *
     * @param key the key to put.
     *
     * @param data the data to put.
     *
     * @param flags the low-level put flags.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     */
    public int put(Dbc cursor, DataThang key, DataThang data, int flags)
        throws DbException {

        if (flags == Db.DB_CURRENT && areDuplicatesOrdered()) {
            // Workaround for two Db issues: 1- with HASH type a put() with
            // different data is allowed, contrary to docs, 2- with non-HASH
            // types an erroror is printed to standard out.
            DataThang temp = new DataThang();
            cursor.get(key, temp, flags);
            if (data.equals(temp)) {
                return 0; // nothing to do, data is already as specified
            } else {
                throw new IllegalArgumentException(
                  "Current data differs from put data with sorted duplicates");
            }
        }
        if (flags == Db.DB_NOOVERWRITE) {
            int err = cursor.get(key, DataThang.getDiscardDataThang(),
                                 Db.DB_SET | env.getWriteLockFlag());
            if (err == 0) {
                return Db.DB_KEYEXIST;
            } else if (err != Db.DB_NOTFOUND) {
                return err;
            }
            flags = 0;
        }
        if (flags == 0) {
            if (areDuplicatesOrdered()) {
                flags = Db.DB_NODUPDATA;
            } else {
                flags = Db.DB_KEYLAST;
            }
        }
        int err = cursor.put(key, data, flags);
        return err;
    }

    /**
     * Performs a general database 'delete' operation.
     *
     * @param key the key to delete.
     *
     * @param flags the low-level delete flags.
     *
     * @return an error or zero for success.
     *
     * @throws DbException if a database problem occurs.
     */
    public int delete(DataThang key, int flags)
        throws DbException {

        Dbc cursor = openCursor(true);
        try {
            int err = cursor.get(key, DataThang.getDiscardDataThang(),
                                 Db.DB_SET | env.getWriteLockFlag());
            if (err == 0) {
                return cursor.delete(0);
            } else {
                return err;
            }
        } finally {
            closeCursor(cursor);
        }
    }

    /**
     * Opens a cursor for this database.
     *
     * @param writeCursor true to open a write cursor in a CDB environment, and
     * ignored for other environments.
     *
     * @return the open cursor.
     *
     * @throws DbException if a database problem occurs.
     */
    public Dbc openCursor(boolean writeCursor)
        throws DbException {

        if (cdbContext != null) {
            CdbThreadContext context = (CdbThreadContext) cdbContext.get();
            List cursors;
            int flags;

            if (writeCursor) {
                cursors = context.writeCursors;
                flags = Db.DB_WRITECURSOR;

                if (context.readCursors.size() > 0) {
                    // although CDB allows opening a write cursor when a read
                    // cursor is open, a self-deadlock will occur if a write is
                    // attempted for a record that is read-locked; we should
                    // avoid self-deadlocks at all costs
                    throw new IllegalStateException(
                      "cannot open CDB write cursor when read cursor is open");
                }
            } else {
                cursors = context.readCursors;
                flags = 0;
            }

            Dbc cursor;
            if (cursors.size() > 0) {
                Dbc other = ((Dbc) cursors.get(0));
                cursor = other.dup(0);
            } else {
                cursor = db.cursor(null, flags);
            }
            cursors.add(cursor);
            return cursor;
        } else {
            return db.cursor(currentTxn(), 0);
        }
    }

    /**
     * Duplicates a cursor for this database.
     *
     * @param writeCursor true to open a write cursor in a CDB environment, and
     * ignored for other environments.
     *
     * @param flags the low-level dup() flags.
     *
     * @return the open cursor.
     *
     * @throws DbException if a database problem occurs.
     */
    public Dbc dupCursor(Dbc cursor, boolean writeCursor, int flags)
        throws DbException {

        if (cdbContext != null) {
            CdbThreadContext context = (CdbThreadContext) cdbContext.get();
            List cursors = writeCursor ? context.writeCursors
                                       : context.readCursors;
            if (!cursors.contains(cursor))
                throw new IllegalStateException("cursor to dup not tracked");
            Dbc newCursor = cursor.dup(flags);
            cursors.add(newCursor);
            return newCursor;
        } else {
            return cursor.dup(flags);
        }
    }

    /**
     * Closes a cursor for this database.
     *
     * @param cursor the cursor to close.
     *
     * @throws DbException if a database problem occurs.
     */
    public void closeCursor(Dbc cursor)
        throws DbException {

        if (cursor == null) return;

        if (cdbContext != null) {
            CdbThreadContext context = (CdbThreadContext) cdbContext.get();

            if (!context.readCursors.remove(cursor) &&
                !context.writeCursors.remove(cursor)) {
                throw new IllegalStateException(
                  "closing CDB cursor that was not known to be open");
            }
        }

        cursor.close();
    }

    private final DbTxn currentTxn() {

        return transactional ? env.getTxn() : null;
    }

    /**
     * Returns a debugging string containing the database name.
     *
     * @return a debugging string.
     */
    public String toString() {

        String val = file;
        if (database != null) file += ' ' + database;
        return toString(this, val);
    }

    static String toString(Object o, String val) {

        String cls = null;
        if (o != null) {
            cls = o.getClass().getName();
            int i = cls.lastIndexOf('.');
            if (i >= 0) cls = cls.substring(i + 1);
        }
        return "[" + cls + ' ' + val + ']';
    }

    private static final class CdbThreadContext {

        private DataDb db;
        private List writeCursors = new ArrayList();
        private List readCursors = new ArrayList();

        CdbThreadContext(DataDb db) {
            this.db = db;
        }
    }
}
