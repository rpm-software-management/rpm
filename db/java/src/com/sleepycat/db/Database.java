/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: Database.java,v 12.8 2007/05/17 15:15:41 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbSequence;
import com.sleepycat.db.internal.Dbc;

public class Database {
    Db db;
    private int autoCommitFlag;
    int rmwFlag;

    /* package */
    Database(final Db db)
        throws DatabaseException {

        this.db = db;
        db.wrapper = this;
        this.autoCommitFlag =
            db.get_transactional() ? DbConstants.DB_AUTO_COMMIT : 0;
        rmwFlag = ((db.get_env().get_open_flags() &
                    DbConstants.DB_INIT_LOCK) != 0) ? DbConstants.DB_RMW : 0;
    }

    public Database(final String filename,
                    final String databaseName,
                    final DatabaseConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        this(DatabaseConfig.checkNull(config).openDatabase(null, null,
            filename, databaseName));
        // Set up dbenv.wrapper
        new Environment(db.get_env());
    }

    public void close(final boolean noSync)
        throws DatabaseException {

        db.close(noSync ? DbConstants.DB_NOSYNC : 0);
    }

    public void close()
        throws DatabaseException {

        close(false);
    }

    public CompactStats compact(final Transaction txn,
                                final DatabaseEntry start,
                                final DatabaseEntry stop,
                                final DatabaseEntry end,
                                CompactConfig config)
        throws DatabaseException {

        config = CompactConfig.checkNull(config);
        CompactStats compact = new CompactStats(config.getFillPercent(),
            config.getTimeout(), config.getMaxPages());
        db.compact((txn == null) ? null : txn.txn,
            start, stop, compact, config.getFlags(), end);
        return compact;
    }

    public Cursor openCursor(final Transaction txn, CursorConfig config)
        throws DatabaseException {

        return new Cursor(this, CursorConfig.checkNull(config).openCursor(
            db, (txn == null) ? null : txn.txn), config);
    }

    public Sequence openSequence(final Transaction txn,
                                 final DatabaseEntry key,
                                 final SequenceConfig config)
        throws DatabaseException {

        return new Sequence(SequenceConfig.checkNull(config).openSequence(
            db, (txn == null) ? null : txn.txn, key), config);
    }

    public void removeSequence(final Transaction txn,
                               final DatabaseEntry key,
                               SequenceConfig config)
        throws DatabaseException {

        config = SequenceConfig.checkNull(config);
        final DbSequence seq = config.openSequence(
            db, (txn == null) ? null : txn.txn, key);
        seq.remove((txn == null) ? null : txn.txn,
            (txn == null && db.get_transactional()) ?
            DbConstants.DB_AUTO_COMMIT | (config.getAutoCommitNoSync() ?
            DbConstants.DB_TXN_NOSYNC : 0) : 0);
    }

    public String getDatabaseFile()
        throws DatabaseException {

        return db.get_filename();
    }

    public String getDatabaseName()
        throws DatabaseException {

        return db.get_dbname();
    }

    public DatabaseConfig getConfig()
        throws DatabaseException {

        return new DatabaseConfig(db);
    }

    public void setConfig(DatabaseConfig config)
        throws DatabaseException {

        config.configureDatabase(db, getConfig());
    }

    public Environment getEnvironment()
        throws DatabaseException {

        return db.get_env().wrapper;
    }

    public CacheFile getCacheFile()
        throws DatabaseException {

        return new CacheFile(db.get_mpf());
    }

    public OperationStatus append(final Transaction txn,
                                  final DatabaseEntry key,
                                  final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.put((txn == null) ? null : txn.txn, key, data,
                DbConstants.DB_APPEND | ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus consume(final Transaction txn,
                                   final DatabaseEntry key,
                                   final DatabaseEntry data,
                                   final boolean wait)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.get((txn == null) ? null : txn.txn,
                key, data,
                (wait ? DbConstants.DB_CONSUME_WAIT : DbConstants.DB_CONSUME) |
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus delete(final Transaction txn,
                                  final DatabaseEntry key)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.del((txn == null) ? null : txn.txn, key,
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus exists(final Transaction txn,
                               final DatabaseEntry key)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.exists((txn == null) ? null : txn.txn, key,
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus get(final Transaction txn,
                               final DatabaseEntry key,
                               final DatabaseEntry data,
                               final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.get((txn == null) ? null : txn.txn,
                key, data,
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public KeyRange getKeyRange(final Transaction txn,
                                final DatabaseEntry key)
        throws DatabaseException {

        final KeyRange range = new KeyRange();
        db.key_range((txn == null) ? null : txn.txn, key, range, 0);
        return range;
    }

    public OperationStatus getSearchBoth(final Transaction txn,
                                         final DatabaseEntry key,
                                         final DatabaseEntry data,
                                         final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.get((txn == null) ? null : txn.txn,
                key, data,
                DbConstants.DB_GET_BOTH |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus getSearchRecordNumber(final Transaction txn,
                                           final DatabaseEntry key,
                                           final DatabaseEntry data,
                                           final LockMode lockMode)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.get((txn == null) ? null : txn.txn,
                key, data,
                DbConstants.DB_SET_RECNO |
                LockMode.getFlag(lockMode) |
                ((data == null) ? 0 : data.getMultiFlag())));
    }

    public OperationStatus put(final Transaction txn,
                               final DatabaseEntry key,
                               final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.put((txn == null) ? null : txn.txn,
                key, data,
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus putNoDupData(final Transaction txn,
                                        final DatabaseEntry key,
                                        final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.put((txn == null) ? null : txn.txn,
                key, data,
                DbConstants.DB_NODUPDATA |
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public OperationStatus putNoOverwrite(final Transaction txn,
                                          final DatabaseEntry key,
                                          final DatabaseEntry data)
        throws DatabaseException {

        return OperationStatus.fromInt(
            db.put((txn == null) ? null : txn.txn,
                key, data,
                DbConstants.DB_NOOVERWRITE |
                ((txn == null) ? autoCommitFlag : 0)));
    }

    public JoinCursor join(final Cursor[] cursList, JoinConfig config)
        throws DatabaseException {

        config = JoinConfig.checkNull(config);

        final Dbc[] dbcList = new Dbc[cursList.length];
        for (int i = 0; i < cursList.length; i++)
            dbcList[i] = (cursList[i] == null) ? null : cursList[i].dbc;

        return new JoinCursor(this,
            db.join(dbcList, config.getFlags()), config);
    }

    public int truncate(final Transaction txn, boolean countRecords)
        throws DatabaseException {

        // XXX: implement countRecords in C
        int count = db.truncate((txn == null) ? null : txn.txn,
            ((txn == null) ? autoCommitFlag : 0));

        return countRecords ? count : -1;
    }

    public DatabaseStats getStats(final Transaction txn, StatsConfig config)
        throws DatabaseException {

        return (DatabaseStats)db.stat((txn == null) ? null : txn.txn,
            StatsConfig.checkNull(config).getFlags());
    }

    public static void remove(final String fileName,
                              final String databaseName,
                              DatabaseConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        final Db db = DatabaseConfig.checkNull(config).createDatabase(null);
        db.remove(fileName, databaseName, 0);
    }

    public static void rename(final String fileName,
                              final String oldDatabaseName,
                              final String newDatabaseName,
                              DatabaseConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        final Db db = DatabaseConfig.checkNull(config).createDatabase(null);
        db.rename(fileName, oldDatabaseName, newDatabaseName, 0);
    }

    public void sync()
        throws DatabaseException {

        db.sync(0);
    }

    public static void upgrade(final String fileName,
                        DatabaseConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        final Db db = DatabaseConfig.checkNull(config).createDatabase(null);
        db.upgrade(fileName,
            config.getSortedDuplicates() ? DbConstants.DB_DUPSORT : 0);
        db.close(0);
    }

    public static boolean verify(final String fileName,
                                 final String databaseName,
                                 final java.io.PrintStream dumpStream,
                                 VerifyConfig verifyConfig,
                                 DatabaseConfig dbConfig)
        throws DatabaseException, java.io.FileNotFoundException {

        final Db db = DatabaseConfig.checkNull(dbConfig).createDatabase(null);
        return db.verify(fileName, databaseName, dumpStream,
            VerifyConfig.checkNull(verifyConfig).getFlags());
    }
}
