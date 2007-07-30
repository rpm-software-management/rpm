/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000,2007 Oracle.  All rights reserved.
 *
 * $Id: DbCompat.java,v 12.10 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.compat;

import java.io.FileNotFoundException;
import java.util.Comparator;

import com.sleepycat.db.Cursor;
import com.sleepycat.db.CursorConfig;
import com.sleepycat.db.Database;
import com.sleepycat.db.DatabaseConfig;
import com.sleepycat.db.DatabaseEntry;
import com.sleepycat.db.DatabaseException;
import com.sleepycat.db.DatabaseType;
import com.sleepycat.db.Environment;
import com.sleepycat.db.EnvironmentConfig;
import com.sleepycat.db.LockDetectMode;
import com.sleepycat.db.LockMode;
import com.sleepycat.db.OperationStatus;
import com.sleepycat.db.SecondaryConfig;
import com.sleepycat.db.SecondaryCursor;
import com.sleepycat.db.SecondaryDatabase;
import com.sleepycat.db.Transaction;
import com.sleepycat.db.TransactionConfig;

/**
 * A minimal set of DB-JE compatibility methods for internal use only.
 * Two versions are maintained in parallel in the DB and JE source trees.
 * Used by the collections package.
 */
public class DbCompat {

    /* Capabilities */

    public static final boolean CDB = true;
    public static final boolean JOIN = true;
    public static final boolean NESTED_TRANSACTIONS = true;
    public static final boolean INSERTION_ORDERED_DUPLICATES = true;
    public static final boolean SEPARATE_DATABASE_FILES = true;
    public static final boolean MEMORY_SUBSYSTEM = true;
    public static final boolean LOCK_SUBSYSTEM = true;
    public static final boolean HASH_METHOD = true;
    public static final boolean RECNO_METHOD = true;
    public static final boolean QUEUE_METHOD = true;
    public static final boolean BTREE_RECNUM_METHOD = true;
    public static final boolean OPTIONAL_READ_UNCOMMITTED = true;
    public static final boolean SECONDARIES = true;
    public static boolean TRANSACTION_RUNNER_PRINT_STACK_TRACES = true;
    public static final boolean DATABASE_COUNT = false;

    /* Methods used by the collections package. */

    public static boolean getInitializeLocking(EnvironmentConfig config) {
        return config.getInitializeLocking();
    }

    public static boolean getInitializeCDB(EnvironmentConfig config) {
        return config.getInitializeCDB();
    }

    public static boolean isTypeBtree(DatabaseConfig dbConfig) {
        return dbConfig.getType() == DatabaseType.BTREE;
    }

    public static boolean isTypeHash(DatabaseConfig dbConfig) {
        return dbConfig.getType() == DatabaseType.HASH;
    }

    public static boolean isTypeQueue(DatabaseConfig dbConfig) {
        return dbConfig.getType() == DatabaseType.QUEUE;
    }

    public static boolean isTypeRecno(DatabaseConfig dbConfig) {
        return dbConfig.getType() == DatabaseType.RECNO;
    }

    public static boolean getBtreeRecordNumbers(DatabaseConfig dbConfig) {
        return dbConfig.getBtreeRecordNumbers();
    }

    public static boolean getReadUncommitted(DatabaseConfig dbConfig) {
        return dbConfig.getReadUncommitted();
    }

    public static boolean getRenumbering(DatabaseConfig dbConfig) {
        return dbConfig.getRenumbering();
    }

    public static boolean getSortedDuplicates(DatabaseConfig dbConfig) {
        return dbConfig.getSortedDuplicates();
    }

    public static boolean getUnsortedDuplicates(DatabaseConfig dbConfig) {
        return dbConfig.getUnsortedDuplicates();
    }

    // XXX Remove this when DB and JE support CursorConfig.cloneConfig
    public static CursorConfig cloneCursorConfig(CursorConfig config) {
        CursorConfig newConfig = new CursorConfig();
        newConfig.setReadCommitted(config.getReadCommitted());
        newConfig.setReadUncommitted(config.getReadUncommitted());
        newConfig.setWriteCursor(config.getWriteCursor());
        return newConfig;
    }

    public static boolean getWriteCursor(CursorConfig config) {
        return config.getWriteCursor();
    }

    public static void setWriteCursor(CursorConfig config, boolean val) {
        config.setWriteCursor(val);
    }

    public static void setRecordNumber(DatabaseEntry entry, int recNum) {
        entry.setRecordNumber(recNum);
    }

    public static int getRecordNumber(DatabaseEntry entry) {
        return entry.getRecordNumber();
    }

    public static String getDatabaseFile(Database db)
        throws DatabaseException {
        return db.getDatabaseFile();
    }

    public static long getDatabaseCount(Database db)
        throws DatabaseException {

        throw new UnsupportedOperationException();
    }

    public static OperationStatus getCurrentRecordNumber(Cursor cursor,
                                                         DatabaseEntry key,
                                                         LockMode lockMode)
        throws DatabaseException {
        return cursor.getRecordNumber(key, lockMode);
    }

    public static OperationStatus getSearchRecordNumber(Cursor cursor,
                                                        DatabaseEntry key,
                                                        DatabaseEntry data,
                                                        LockMode lockMode)
        throws DatabaseException {
        return cursor.getSearchRecordNumber(key, data, lockMode);
    }

    public static OperationStatus getSearchRecordNumber(SecondaryCursor cursor,
                                                        DatabaseEntry key,
                                                        DatabaseEntry pKey,
                                                        DatabaseEntry data,
                                                        LockMode lockMode)
        throws DatabaseException {
        return cursor.getSearchRecordNumber(key, pKey, data, lockMode);
    }

    public static OperationStatus putAfter(Cursor cursor,
                                           DatabaseEntry key,
                                           DatabaseEntry data)
        throws DatabaseException {
        return cursor.putAfter(key, data);
    }

    public static OperationStatus putBefore(Cursor cursor,
                                            DatabaseEntry key,
                                            DatabaseEntry data)
        throws DatabaseException {
        return cursor.putBefore(key, data);
    }

    public static OperationStatus append(Database db,
                                         Transaction txn,
                                         DatabaseEntry key,
                                         DatabaseEntry data)
        throws DatabaseException {
        return db.append(txn, key, data);
    }

    public static Transaction getThreadTransaction(Environment env)
	throws DatabaseException {
        return null;
    }

    /* Methods used by the collections tests. */

    public static void setInitializeCache(EnvironmentConfig config,
                                          boolean val) {
        config.setInitializeCache(val);
    }

    public static void setInitializeLocking(EnvironmentConfig config,
                                            boolean val) {
        config.setInitializeLocking(val);
    }

    public static void setInitializeCDB(EnvironmentConfig config,
                                        boolean val) {
        config.setInitializeCDB(val);
    }

    public static void setLockDetectModeOldest(EnvironmentConfig config) {

        config.setLockDetectMode(LockDetectMode.OLDEST);
    }

    public static void setBtreeComparator(DatabaseConfig dbConfig,
                                          Comparator comparator) {
        dbConfig.setBtreeComparator(comparator);
    }

    public static void setTypeBtree(DatabaseConfig dbConfig) {
        dbConfig.setType(DatabaseType.BTREE);
    }

    public static void setTypeHash(DatabaseConfig dbConfig) {
        dbConfig.setType(DatabaseType.HASH);
    }

    public static void setTypeRecno(DatabaseConfig dbConfig) {
        dbConfig.setType(DatabaseType.RECNO);
    }

    public static void setTypeQueue(DatabaseConfig dbConfig) {
        dbConfig.setType(DatabaseType.QUEUE);
    }

    public static void setBtreeRecordNumbers(DatabaseConfig dbConfig,
                                             boolean val) {
        dbConfig.setBtreeRecordNumbers(val);
    }

    public static void setReadUncommitted(DatabaseConfig dbConfig,
                                          boolean val) {
        dbConfig.setReadUncommitted(val);
    }

    public static void setRenumbering(DatabaseConfig dbConfig,
                                      boolean val) {
        dbConfig.setRenumbering(val);
    }

    public static void setSortedDuplicates(DatabaseConfig dbConfig,
                                           boolean val) {
        dbConfig.setSortedDuplicates(val);
    }

    public static void setUnsortedDuplicates(DatabaseConfig dbConfig,
                                             boolean val) {
        dbConfig.setUnsortedDuplicates(val);
    }

    public static void setRecordLength(DatabaseConfig dbConfig, int val) {
        dbConfig.setRecordLength(val);
    }

    public static void setRecordPad(DatabaseConfig dbConfig, int val) {
        dbConfig.setRecordPad(val);
    }

    public static Database openDatabase(Environment env,
                                        Transaction txn,
                                        String file,
                                        String name,
                                        DatabaseConfig config)
        throws DatabaseException, FileNotFoundException {
        return env.openDatabase(txn, file, name, config);
    }

    public static SecondaryDatabase
                  openSecondaryDatabase(Environment env,
                                        Transaction txn,
                                        String file,
                                        String name,
                                        Database primary,
                                        SecondaryConfig config)
        throws DatabaseException, FileNotFoundException {
        return env.openSecondaryDatabase(txn, file, name, primary, config);
    }
}
