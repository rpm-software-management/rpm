/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: Environment.java,v 12.22 2007/07/06 00:22:54 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;

public class Environment {
    private DbEnv dbenv;
    private int autoCommitFlag;

    /* package */
    Environment(final DbEnv dbenv)
        throws DatabaseException {

        this.dbenv = dbenv;
        dbenv.wrapper = this;
    }

    public Environment(final java.io.File home, EnvironmentConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        this(EnvironmentConfig.checkNull(config).openEnvironment(home));
        this.autoCommitFlag =
            ((dbenv.get_open_flags() & DbConstants.DB_INIT_TXN) == 0) ? 0 :
                DbConstants.DB_AUTO_COMMIT;
    }

    public void close()
        throws DatabaseException {

        dbenv.close(0);
    }

    /* package */
    DbEnv unwrap() {
        return dbenv;
    }

    public static void remove(final java.io.File home,
                              final boolean force,
                              EnvironmentConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        config = EnvironmentConfig.checkNull(config);
        int flags = force ? DbConstants.DB_FORCE : 0;
        flags |= config.getUseEnvironment() ?
            DbConstants.DB_USE_ENVIRON : 0;
        flags |= config.getUseEnvironmentRoot() ?
            DbConstants.DB_USE_ENVIRON_ROOT : 0;
        final DbEnv dbenv = config.createEnvironment();
        dbenv.remove((home == null) ? null : home.toString(), flags);
    }

    public void setConfig(final EnvironmentConfig config)
        throws DatabaseException {

        config.configureEnvironment(dbenv, new EnvironmentConfig(dbenv));
    }

    public EnvironmentConfig getConfig()
        throws DatabaseException {

        return new EnvironmentConfig(dbenv);
    }

    /* Manage databases. */
    public Database openDatabase(final Transaction txn,
                                 final String fileName,
                                 final String databaseName,
                                 DatabaseConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        return new Database(
            DatabaseConfig.checkNull(config).openDatabase(dbenv,
                (txn == null) ? null : txn.txn,
                fileName, databaseName));
    }

    public SecondaryDatabase openSecondaryDatabase(
            final Transaction txn,
            final String fileName,
            final String databaseName,
            final Database primaryDatabase,
            SecondaryConfig config)
        throws DatabaseException, java.io.FileNotFoundException {

        return new SecondaryDatabase(
            SecondaryConfig.checkNull(config).openSecondaryDatabase(
                dbenv, (txn == null) ? null : txn.txn,
                fileName, databaseName, primaryDatabase.db),
            primaryDatabase);
    }

    public void removeDatabase(final Transaction txn,
                               final String fileName,
                               final String databaseName)
        throws DatabaseException, java.io.FileNotFoundException {

        dbenv.dbremove((txn == null) ? null : txn.txn,
            fileName, databaseName,
            (txn == null) ? autoCommitFlag : 0);
    }

    public void renameDatabase(final Transaction txn,
                               final String fileName,
                               final String databaseName,
                               final String newName)
        throws DatabaseException, java.io.FileNotFoundException {

        dbenv.dbrename((txn == null) ? null : txn.txn,
            fileName, databaseName, newName,
            (txn == null) ? autoCommitFlag : 0);
    }

    public java.io.File getHome()
        throws DatabaseException {

        String home = dbenv.get_home();
        return (home == null) ? null : new java.io.File(home);
    }

    /* Cache management. */
    public int trickleCacheWrite(int percent)
        throws DatabaseException {

        return dbenv.memp_trickle(percent);
    }

    /* Locking */
    public int detectDeadlocks(LockDetectMode mode)
        throws DatabaseException {

        return dbenv.lock_detect(0, mode.getFlag());
    }

    public Lock getLock(int locker,
                        boolean noWait,
                        DatabaseEntry object,
                        LockRequestMode mode)
        throws DatabaseException {

        return Lock.wrap(
            dbenv.lock_get(locker, noWait ? DbConstants.DB_LOCK_NOWAIT : 0,
                object, mode.getFlag()));
    }

    public void putLock(Lock lock)
        throws DatabaseException {

        dbenv.lock_put(lock.unwrap());
    }

    public int createLockerID()
        throws DatabaseException {

        return dbenv.lock_id();
    }

    public void freeLockerID(int id)
        throws DatabaseException {

        dbenv.lock_id_free(id);
    }

    public void lockVector(int locker, boolean noWait, LockRequest[] list)
        throws DatabaseException {

        dbenv.lock_vec(locker, noWait ? DbConstants.DB_LOCK_NOWAIT : 0,
            list, 0, list.length);
    }

    /* Logging */
    public LogCursor openLogCursor()
        throws DatabaseException {

        return LogCursor.wrap(dbenv.log_cursor(0));
    }

    public String getLogFileName(LogSequenceNumber lsn)
        throws DatabaseException {

        return dbenv.log_file(lsn);
    }

    /* Replication support */
    public void startReplication(DatabaseEntry cdata, boolean master)
        throws DatabaseException {

        dbenv.rep_start(cdata,
            master ? DbConstants.DB_REP_MASTER : DbConstants.DB_REP_CLIENT);
    }

    public void electReplicationMaster(int nsites, int nvotes)
        throws DatabaseException {
        dbenv.rep_elect(nsites, nvotes, 0 /* unused flags */);
    }

    public void flushReplication()
        throws DatabaseException {

        dbenv.rep_flush();
    }

    public ReplicationStatus processReplicationMessage(DatabaseEntry control,
                                                       DatabaseEntry rec,
                                                       int envid)
        throws DatabaseException {
        // Create a new entry so that rec isn't overwritten
        final DatabaseEntry cdata =
            new DatabaseEntry(rec.getData(), rec.getOffset(), rec.getSize());
        final LogSequenceNumber lsn = new LogSequenceNumber();
        final int ret =
            dbenv.rep_process_message(control, cdata, envid, lsn);
        return ReplicationStatus.getStatus(ret, cdata, envid, lsn);
    }

    public void setReplicationConfig(ReplicationConfig config, boolean onoff)
        throws DatabaseException {

        dbenv.rep_set_config(config.getFlag(), onoff);
    }

    public boolean getReplicationConfig(ReplicationConfig config)
        throws DatabaseException {

        return dbenv.rep_get_config(config.getFlag());
    }

    public void setReplicationTimeout(
        final ReplicationTimeoutType type, final int replicationTimeout)
        throws DatabaseException {
        dbenv.rep_set_timeout(type.getId(), replicationTimeout);
    }

    public int getReplicationTimeout(final ReplicationTimeoutType type)
        throws DatabaseException {
        return dbenv.rep_get_timeout(type.getId());
    }

    public void syncReplication() throws DatabaseException {
        dbenv.rep_sync(0);
    }

    /* Replication Manager interface */
    public void replicationManagerStart(
        int nthreads, ReplicationManagerStartPolicy disp)
        throws DatabaseException {
        dbenv.repmgr_start(nthreads, disp.getId());
    }

    /* Statistics */
    public CacheStats getCacheStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.memp_stat(StatsConfig.checkNull(config).getFlags());
    }

    public CacheFileStats[] getCacheFileStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.memp_fstat(StatsConfig.checkNull(config).getFlags());
    }

    public LogStats getLogStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.log_stat(StatsConfig.checkNull(config).getFlags());
    }

    public ReplicationStats getReplicationStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.rep_stat(StatsConfig.checkNull(config).getFlags());
    }

    public ReplicationManagerStats getReplicationManagerStats(
        StatsConfig config)
        throws DatabaseException {

        return dbenv.repmgr_stat(StatsConfig.checkNull(config).getFlags());
    }

    public LockStats getLockStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.lock_stat(StatsConfig.checkNull(config).getFlags());
    }

    public MutexStats getMutexStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.mutex_stat(StatsConfig.checkNull(config).getFlags());
    }

    public TransactionStats getTransactionStats(StatsConfig config)
        throws DatabaseException {

        return dbenv.txn_stat(StatsConfig.checkNull(config).getFlags());
    }

    /* Transaction management */
    public Transaction beginCDSGroup() throws DatabaseException {

        return new Transaction(dbenv.cdsgroup_begin());
    }

    public Transaction beginTransaction(final Transaction parent,
                                        TransactionConfig config)
        throws DatabaseException {

        return new Transaction(
            TransactionConfig.checkNull(config).beginTransaction(dbenv,
                (parent == null) ? null : parent.txn));
    }

    public void checkpoint(CheckpointConfig config)
        throws DatabaseException {

        CheckpointConfig.checkNull(config).runCheckpoint(dbenv);
    }

    public void logFlush(LogSequenceNumber lsn)
        throws DatabaseException {

        dbenv.log_flush(lsn);
    }

    public LogSequenceNumber logPut(DatabaseEntry data, boolean flush)
        throws DatabaseException {

        final LogSequenceNumber lsn = new LogSequenceNumber();
        dbenv.log_put(lsn, data, flush ? DbConstants.DB_FLUSH : 0);
        return lsn;
    }

    public void logPrint(Transaction txn, String message)
        throws DatabaseException {

        dbenv.log_print((txn == null) ? null : txn.txn, message);
    }

    public java.io.File[] getArchiveLogFiles(boolean includeInUse)
        throws DatabaseException {

        final String[] logNames = dbenv.log_archive(DbConstants.DB_ARCH_ABS |
                (includeInUse ? DbConstants.DB_ARCH_LOG : 0));
        final int len = (logNames == null) ? 0 : logNames.length;
        final java.io.File[] logFiles = new java.io.File[len];
        for (int i = 0; i < len; i++)
            logFiles[i] = new java.io.File(logNames[i]);
        return logFiles;
    }

    public java.io.File[] getArchiveDatabases()
        throws DatabaseException {

        final String home = dbenv.get_home();
        final String[] dbNames = dbenv.log_archive(DbConstants.DB_ARCH_DATA);
        final int len = (dbNames == null) ? 0 : dbNames.length;
        final java.io.File[] dbFiles = new java.io.File[len];
        for (int i = 0; i < len; i++)
            dbFiles[i] = new java.io.File(home, dbNames[i]);
        return dbFiles;
    }

    public void removeOldLogFiles()
        throws DatabaseException {

        dbenv.log_archive(DbConstants.DB_ARCH_REMOVE);
    }

    public PreparedTransaction[] recover(final int count,
                                         final boolean continued)
        throws DatabaseException {

        return dbenv.txn_recover(count,
            continued ? DbConstants.DB_NEXT : DbConstants.DB_FIRST);
    }

    public void resetFileID(final String filename, boolean encrypted)
        throws DatabaseException {

        dbenv.fileid_reset(filename, encrypted ? DbConstants.DB_ENCRYPT : 0);
    }

    public void resetLogSequenceNumber(final String filename, boolean encrypted)
        throws DatabaseException {

        dbenv.lsn_reset(filename, encrypted ? DbConstants.DB_ENCRYPT : 0);
    }

    /* Panic the environment, or stop a panic. */
    public void panic(boolean onoff)
        throws DatabaseException {

        dbenv.set_flags(DbConstants.DB_PANIC_ENVIRONMENT, onoff);
    }

    /* Version information */
    public static String getVersionString() {
        return DbEnv.get_version_string();
    }

    public static int getVersionMajor() {
        return DbEnv.get_version_major();
    }

    public static int getVersionMinor() {
        return DbEnv.get_version_minor();
    }

    public static int getVersionPatch() {
        return DbEnv.get_version_patch();
    }
}
