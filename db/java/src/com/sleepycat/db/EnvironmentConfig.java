/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: EnvironmentConfig.java,v 1.15 2004/11/05 00:50:54 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;

public class EnvironmentConfig implements Cloneable {
    /*
     * For internal use, to allow null as a valid value for
     * the config parameter.
     */
    public static final EnvironmentConfig DEFAULT = new EnvironmentConfig();

    /* package */
    static EnvironmentConfig checkNull(EnvironmentConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    /* Parameters */
    private int mode = 0644;
    private int cacheCount = 0;
    private long cacheSize = 0L;
    private java.util.Vector dataDirs = new java.util.Vector();
    private int envid = 0;
    private java.io.OutputStream errorStream = null;
    private String errorPrefix = null;
    private byte[][] lockConflicts = null;
    private LockDetectMode lockDetectMode = LockDetectMode.NONE;
    private int maxLocks = 0;
    private int maxLockers = 0;
    private int maxLockObjects = 0;
    private int maxLogFileSize = 0;
    private int logBufferSize = 0;
    private java.io.OutputStream messageStream = null;
    private java.io.File logDirectory = null;
    private int logRegionSize = 0;
    private long mmapSize = 0L;
    private String password = null;
    private long replicationLimit = 0L;
    private String rpcServer = null;
    private long rpcClientTimeout = 0L;
    private long rpcServerTimeout = 0L;
    private long segmentId = 0L;
    private int testAndSetSpins = 0;
    private long lockTimeout = 0L;
    private int txnMaxActive = 0;
    private long txnTimeout = 0L;
    private java.util.Date txnTimestamp = null;
    private String temporaryDirectory = null;

    /* Open flags */
    private boolean allowCreate = false;
    private boolean initializeCache = false;
    private boolean initializeCDB = false;
    private boolean initializeLocking = false;
    private boolean initializeLogging = false;
    private boolean initializeReplication = false;
    private boolean joinEnvironment = false;
    private boolean lockDown = false;
    private boolean isPrivate = false;
    private boolean readOnly = false;
    private boolean runRecovery = false;
    private boolean runFatalRecovery = false;
    private boolean systemMemory = false;
    private boolean threaded = true;  // Handles are threaded by default in Java
    private boolean transactional = false;
    private boolean useEnvironment = false;
    private boolean useEnvironmentRoot = false;

    /* Flags */
    private boolean cdbLockAllDatabases = false;
    private boolean directDatabaseIO = false;
    private boolean directLogIO = false;
    private boolean dsyncLog = false;
    private boolean initializeRegions = false;
    private boolean logAutoRemove = false;
    private boolean logInMemory = false;
    private boolean noLocking = false;
    private boolean noMMap = false;
    private boolean noPanic = false;
    private boolean overwrite = false;
    private boolean txnNoSync = false;
    private boolean txnNotDurable = false;
    private boolean txnWriteNoSync = false;
    private boolean yieldCPU = false;

    /* Verbose Flags */
    private boolean verboseDeadlock = false;
    private boolean verboseRecovery = false;
    private boolean verboseReplication = false;
    private boolean verboseWaitsFor = false;

    /* Callbacks */
    private ErrorHandler errorHandler = null;
    private FeedbackHandler feedbackHandler = null;
    private LogRecordHandler logRecordHandler = null;
    private MessageHandler messageHandler = null;
    private PanicHandler panicHandler = null;
    private ReplicationTransport replicationTransport = null;

    public EnvironmentConfig() {
    }

    public void setAllowCreate(final boolean allowCreate) {
        this.allowCreate = allowCreate;
    }

    public boolean getAllowCreate() {
        return allowCreate;
    }

    public void setCacheSize(final long cacheSize) {
        this.cacheSize = cacheSize;
    }

    public long getCacheSize() {
        return cacheSize;
    }

    public void setCacheCount(final int cacheCount) {
        this.cacheCount = cacheCount;
    }

    public int getCacheCount() {
        return cacheCount;
    }

    public void setCDBLockAllDatabases(final boolean cdbLockAllDatabases) {
        this.cdbLockAllDatabases = cdbLockAllDatabases;
    }

    public boolean getCDBLockAllDatabases() {
        return cdbLockAllDatabases;
    }

    public void addDataDir(final String dataDir) {
        this.dataDirs.add(dataDir);
    }

    public String[] getDataDirs() {
        final String[] dirs = new String[dataDirs.size()];
        dataDirs.copyInto(dirs);
        return dirs;
    }

    public void setDirectDatabaseIO(final boolean directDatabaseIO) {
        this.directDatabaseIO = directDatabaseIO;
    }

    public boolean getDirectDatabaseIO() {
        return directDatabaseIO;
    }

    public void setDirectLogIO(final boolean directLogIO) {
        this.directLogIO = directLogIO;
    }

    public boolean getDirectLogIO() {
        return directLogIO;
    }

    public void setDsyncLog(final boolean dsyncLog) {
        this.dsyncLog = dsyncLog;
    }

    public boolean getDsyncLog() {
        return dsyncLog;
    }

    public void setEncrypted(final String password) {
        this.password = password;
    }

    public boolean getEncrypted() {
        return (password != null);
    }

    public void setErrorHandler(final ErrorHandler errorHandler) {
        this.errorHandler = errorHandler;
    }

    public ErrorHandler getErrorHandler() {
        return errorHandler;
    }

    public void setErrorPrefix(final String errorPrefix) {
        this.errorPrefix = errorPrefix;
    }

    public String getErrorPrefix() {
        return errorPrefix;
    }

    public void setErrorStream(final java.io.OutputStream errorStream) {
        this.errorStream = errorStream;
    }

    public java.io.OutputStream getErrorStream() {
        return errorStream;
    }

    public void setFeedbackHandler(final FeedbackHandler feedbackHandler) {
        this.feedbackHandler = feedbackHandler;
    }

    public FeedbackHandler getFeedbackHandler() {
        return feedbackHandler;
    }

    public void setInitializeCache(final boolean initializeCache) {
        this.initializeCache = initializeCache;
    }

    public boolean getInitializeCache() {
        return initializeCache;
    }

    public void setInitializeCDB(final boolean initializeCDB) {
        this.initializeCDB = initializeCDB;
    }

    public boolean getInitializeCDB() {
        return initializeCDB;
    }

    public void setInitializeLocking(final boolean initializeLocking) {
        this.initializeLocking = initializeLocking;
    }

    public boolean getInitializeLocking() {
        return initializeLocking;
    }

    public void setInitializeLogging(final boolean initializeLogging) {
        this.initializeLogging = initializeLogging;
    }

    public boolean getInitializeLogging() {
        return initializeLogging;
    }

    public void setInitializeRegions(final boolean initializeRegions) {
        this.initializeRegions = initializeRegions;
    }

    public boolean getInitializeRegions() {
        return initializeRegions;
    }

    public void setInitializeReplication(final boolean initializeReplication) {
        this.initializeReplication = initializeReplication;
    }

    public boolean getInitializeReplication() {
        return initializeReplication;
    }

    public void setJoinEnvironment(final boolean joinEnvironment) {
        this.joinEnvironment = joinEnvironment;
    }

    public boolean getJoinEnvironment() {
        return joinEnvironment;
    }

    public void setLockConflicts(final byte[][] lockConflicts) {
        this.lockConflicts = lockConflicts;
    }

    public byte[][] getLockConflicts() {
        return lockConflicts;
    }

    public void setLockDetectMode(final LockDetectMode lockDetectMode) {
        this.lockDetectMode = lockDetectMode;
    }

    public LockDetectMode getLockDetectMode() {
        return lockDetectMode;
    }

    public void setLockDown(final boolean lockDown) {
        this.lockDown = lockDown;
    }

    public boolean getLockDown() {
        return lockDown;
    }

    public void setLockTimeout(final long lockTimeout) {
        this.lockTimeout = lockTimeout;
    }

    public long getLockTimeout() {
        return lockTimeout;
    }

    public void setLogAutoRemove(final boolean logAutoRemove) {
        this.logAutoRemove = logAutoRemove;
    }

    public boolean getLogAutoRemove() {
        return logAutoRemove;
    }

    public void setLogInMemory(final boolean logInMemory) {
        this.logInMemory = logInMemory;
    }

    public boolean getLogInMemory() {
        return logInMemory;
    }

    public void setLogRecordHandler(final LogRecordHandler logRecordHandler) {
        this.logRecordHandler = logRecordHandler;
    }

    public LogRecordHandler getLogRecordHandler() {
        return logRecordHandler;
    }

    public void setMaxLocks(final int maxLocks) {
        this.maxLocks = maxLocks;
    }

    public int getMaxLocks() {
        return maxLocks;
    }

    public void setMaxLockers(final int maxLockers) {
        this.maxLockers = maxLockers;
    }

    public int getMaxLockers() {
        return maxLockers;
    }

    public void setMaxLockObjects(final int maxLockObjects) {
        this.maxLockObjects = maxLockObjects;
    }

    public int getMaxLockObjects() {
        return maxLockObjects;
    }

    public void setMaxLogFileSize(final int maxLogFileSize) {
        this.maxLogFileSize = maxLogFileSize;
    }

    public int getMaxLogFileSize() {
        return maxLogFileSize;
    }

    public void setLogBufferSize(final int logBufferSize) {
        this.logBufferSize = logBufferSize;
    }

    public int getLogBufferSize() {
        return logBufferSize;
    }

    public void setLogDirectory(final java.io.File logDirectory) {
        this.logDirectory = logDirectory;
    }

    public java.io.File getLogDirectory() {
        return logDirectory;
    }

    public void setLogRegionSize(final int logRegionSize) {
        this.logRegionSize = logRegionSize;
    }

    public int getLogRegionSize() {
        return logRegionSize;
    }

    public void setMessageHandler(final MessageHandler messageHandler) {
        this.messageHandler = messageHandler;
    }

    public MessageHandler getMessageHandler() {
        return messageHandler;
    }

    public void setMessageStream(final java.io.OutputStream messageStream) {
        this.messageStream = messageStream;
    }

    public java.io.OutputStream getMessageStream() {
        return messageStream;
    }

    public void setMMapSize(final long mmapSize) {
        this.mmapSize = mmapSize;
    }

    public long getMMapSize() {
        return mmapSize;
    }

    public void setMode(final int mode) {
        this.mode = mode;
    }

    public long getMode() {
        return mode;
    }

    public void setNoLocking(final boolean noLocking) {
        this.noLocking = noLocking;
    }

    public boolean getNoLocking() {
        return noLocking;
    }

    public void setNoMMap(final boolean noMMap) {
        this.noMMap = noMMap;
    }

    public boolean getNoMMap() {
        return noMMap;
    }

    public void setNoPanic(final boolean noPanic) {
        this.noPanic = noPanic;
    }

    public boolean getNoPanic() {
        return noPanic;
    }

    public void setOverwrite(final boolean overwrite) {
        this.overwrite = overwrite;
    }

    public boolean getOverwrite() {
        return overwrite;
    }

    public void setPanicHandler(final PanicHandler panicHandler) {
        this.panicHandler = panicHandler;
    }

    public PanicHandler getPanicHandler() {
        return panicHandler;
    }

    public void setPrivate(final boolean isPrivate) {
        this.isPrivate = isPrivate;
    }

    public boolean getPrivate() {
        return isPrivate;
    }

    public boolean getReadOnly() {
        return readOnly;
    }

    public void setReadOnly(final boolean readOnly) {
        this.readOnly = readOnly;
    }

    public void setReplicationLimit(final long replicationLimit) {
        this.replicationLimit = replicationLimit;
    }

    public long getReplicationLimit() {
        return replicationLimit;
    }

    public void setReplicationTransport(final int envid,
        final ReplicationTransport replicationTransport) {

        this.envid = envid;
        this.replicationTransport = replicationTransport;
    }

    public ReplicationTransport getReplicationTransport() {
        return replicationTransport;
    }

    public void setRunFatalRecovery(final boolean runFatalRecovery) {
        this.runFatalRecovery = runFatalRecovery;
    }

    public boolean getRunFatalRecovery() {
        return runFatalRecovery;
    }

    public void setRunRecovery(final boolean runRecovery) {
        this.runRecovery = runRecovery;
    }

    public boolean getRunRecovery() {
        return runRecovery;
    }

    public void setSystemMemory(final boolean systemMemory) {
        this.systemMemory = systemMemory;
    }

    public boolean getSystemMemory() {
        return systemMemory;
    }

    public void setRPCServer(final String rpcServer,
                             final long rpcClientTimeout,
                             final long rpcServerTimeout) {
        this.rpcServer = rpcServer;
        this.rpcClientTimeout = rpcClientTimeout;
        this.rpcServerTimeout = rpcServerTimeout;

        // Turn off threading for RPC client handles.
        this.threaded = false;
    }

    public void setSegmentId(final long segmentId) {
        this.segmentId = segmentId;
    }

    public long getSegmentId() {
        return segmentId;
    }

    public void setTemporaryDirectory(final String temporaryDirectory) {
        this.temporaryDirectory = temporaryDirectory;
    }

    public String getTemporaryDirectory() {
        return temporaryDirectory;
    }

    public void setTestAndSetSpins(final int testAndSetSpins) {
        this.testAndSetSpins = testAndSetSpins;
    }

    public int getTestAndSetSpins() {
        return testAndSetSpins;
    }

    public void setThreaded(final boolean threaded) {
        this.threaded = threaded;
    }

    public boolean getThreaded() {
        return threaded;
    }

    public void setTransactional(final boolean transactional) {
        this.transactional = transactional;
    }

    public boolean getTransactional() {
        return transactional;
    }

    public void setTxnNoSync(final boolean txnNoSync) {
        this.txnNoSync = txnNoSync;
    }

    public boolean getTxnNoSync() {
        return txnNoSync;
    }

    public void setTxnNotDurable(final boolean txnNotDurable) {
        this.txnNotDurable = txnNotDurable;
    }

    public boolean getTxnNotDurable() {
        return txnNotDurable;
    }

    public void setTxnMaxActive(final int txnMaxActive) {
        this.txnMaxActive = txnMaxActive;
    }

    public int getTxnMaxActive() {
        return txnMaxActive;
    }

    public void setTxnTimeout(final long txnTimeout) {
        this.txnTimeout = txnTimeout;
    }

    public long getTxnTimeout() {
        return txnTimeout;
    }

    public void setTxnTimestamp(final java.util.Date txnTimestamp) {
        this.txnTimestamp = txnTimestamp;
    }

    public java.util.Date getTxnTimestamp() {
        return txnTimestamp;
    }

    public void setTxnWriteNoSync(final boolean txnWriteNoSync) {
        this.txnWriteNoSync = txnWriteNoSync;
    }

    public boolean getTxnWriteNoSync() {
        return txnWriteNoSync;
    }

    public void setUseEnvironment(final boolean useEnvironment) {
        this.useEnvironment = useEnvironment;
    }

    public boolean getUseEnvironment() {
        return useEnvironment;
    }

    public void setUseEnvironmentRoot(final boolean useEnvironmentRoot) {
        this.useEnvironmentRoot = useEnvironmentRoot;
    }

    public boolean getUseEnvironmentRoot() {
        return useEnvironmentRoot;
    }

    public void setVerboseDeadlock(final boolean verboseDeadlock) {
        this.verboseDeadlock = verboseDeadlock;
    }

    public boolean getVerboseDeadlock() {
        return verboseDeadlock;
    }

    public void setVerboseRecovery(final boolean verboseRecovery) {
        this.verboseRecovery = verboseRecovery;
    }

    public boolean getVerboseRecovery() {
        return verboseRecovery;
    }

    public void setVerboseReplication(final boolean verboseReplication) {
        this.verboseReplication = verboseReplication;
    }

    public boolean getVerboseReplication() {
        return verboseReplication;
    }

    public void setVerboseWaitsFor(final boolean verboseWaitsFor) {
        this.verboseWaitsFor = verboseWaitsFor;
    }

    public boolean getVerboseWaitsFor() {
        return verboseWaitsFor;
    }

    public void setYieldCPU(final boolean yieldCPU) {
        this.yieldCPU = yieldCPU;
    }

    public boolean getYieldCPU() {
        return yieldCPU;
    }

    private boolean lockConflictsEqual(byte[][] lc1, byte[][]lc2) {
        if (lc1 == lc2)
            return true;
        if (lc1 == null || lc2 == null || lc1.length != lc2.length)
            return false;
        for (int i = 0; i < lc1.length; i++) {
            if (lc1[i].length != lc2[i].length)
                return false;
            for (int j = 0; j < lc1[i].length; j++)
                if (lc1[i][j] != lc2[i][j])
                    return false;
        }
        return true;
    }

    /* package */
    DbEnv openEnvironment(final java.io.File home)
        throws DatabaseException, java.io.FileNotFoundException {

        final DbEnv dbenv = createEnvironment();
        int openFlags = 0;

        openFlags |= allowCreate ? DbConstants.DB_CREATE : 0;
        openFlags |= initializeCache ? DbConstants.DB_INIT_MPOOL : 0;
        openFlags |= initializeCDB ? DbConstants.DB_INIT_CDB : 0;
        openFlags |= initializeLocking ? DbConstants.DB_INIT_LOCK : 0;
        openFlags |= initializeLogging ? DbConstants.DB_INIT_LOG : 0;
        openFlags |= initializeReplication ? DbConstants.DB_INIT_REP : 0;
        openFlags |= joinEnvironment ? DbConstants.DB_JOINENV : 0;
        openFlags |= lockDown ? DbConstants.DB_LOCKDOWN : 0;
        openFlags |= isPrivate ? DbConstants.DB_PRIVATE : 0;
        openFlags |= readOnly ? DbConstants.DB_RDONLY : 0;
        openFlags |= runRecovery ? DbConstants.DB_RECOVER : 0;
        openFlags |= runFatalRecovery ? DbConstants.DB_RECOVER_FATAL : 0;
        openFlags |= systemMemory ? DbConstants.DB_SYSTEM_MEM : 0;
        openFlags |= threaded ? DbConstants.DB_THREAD : 0;
        openFlags |= transactional ? DbConstants.DB_INIT_TXN : 0;
        openFlags |= useEnvironment ? DbConstants.DB_USE_ENVIRON : 0;
        openFlags |= useEnvironmentRoot ? DbConstants.DB_USE_ENVIRON_ROOT : 0;

        boolean succeeded = false;
        try {
            dbenv.open((home == null) ? null : home.toString(),
                openFlags, mode);
            succeeded = true;
            return dbenv;
        } finally {
            if (!succeeded)
                try {
                    dbenv.close(0);
                } catch (Throwable t) {
                    // Ignore it -- an exception is already in flight.
                }
        }
    }


    /* package */
    DbEnv createEnvironment()
        throws DatabaseException {

        int createFlags = 0;

        if (rpcServer != null)
                createFlags |= DbConstants.DB_RPCCLIENT;

        final DbEnv dbenv = new DbEnv(createFlags);
        configureEnvironment(dbenv, DEFAULT);
        return dbenv;
    }

    /* package */
    void configureEnvironment(final DbEnv dbenv,
                              final EnvironmentConfig oldConfig)
        throws DatabaseException {

        if (errorHandler != oldConfig.errorHandler)
            dbenv.set_errcall(errorHandler);
        if (errorPrefix != oldConfig.errorPrefix &&
            errorPrefix != null && !errorPrefix.equals(oldConfig.errorPrefix))
            dbenv.set_errpfx(errorPrefix);
        if (errorStream != oldConfig.errorStream)
            dbenv.set_error_stream(errorStream);

        if (rpcServer != oldConfig.rpcServer ||
            rpcClientTimeout != oldConfig.rpcClientTimeout ||
            rpcServerTimeout != oldConfig.rpcServerTimeout)
            dbenv.set_rpc_server(null, rpcServer,
                rpcClientTimeout, rpcServerTimeout, 0);

        // We always set DB_TIME_NOTGRANTED in the Java API, because
        // LockNotGrantedException extends DeadlockException, so there's no
        // reason why an application would prefer one to the other.
        int onFlags = DbConstants.DB_TIME_NOTGRANTED;
        int offFlags = 0;

        if (cdbLockAllDatabases && !oldConfig.cdbLockAllDatabases)
            onFlags |= DbConstants.DB_CDB_ALLDB;
        if (!cdbLockAllDatabases && oldConfig.cdbLockAllDatabases)
            offFlags |= DbConstants.DB_CDB_ALLDB;

        if (directDatabaseIO && !oldConfig.directDatabaseIO)
            onFlags |= DbConstants.DB_DIRECT_DB;
        if (!directDatabaseIO && oldConfig.directDatabaseIO)
            offFlags |= DbConstants.DB_DIRECT_DB;

        if (directLogIO && !oldConfig.directLogIO)
            onFlags |= DbConstants.DB_DIRECT_LOG;
        if (!directLogIO && oldConfig.directLogIO)
            offFlags |= DbConstants.DB_DIRECT_LOG;

        if (dsyncLog && !oldConfig.dsyncLog)
            onFlags |= DbConstants.DB_DSYNC_LOG;
        if (!dsyncLog && oldConfig.dsyncLog)
            offFlags |= DbConstants.DB_DSYNC_LOG;

        if (initializeRegions && !oldConfig.initializeRegions)
            onFlags |= DbConstants.DB_REGION_INIT;
        if (!initializeRegions && oldConfig.initializeRegions)
            offFlags |= DbConstants.DB_REGION_INIT;

        if (logAutoRemove && !oldConfig.logAutoRemove)
            onFlags |= DbConstants.DB_LOG_AUTOREMOVE;
        if (!logAutoRemove && oldConfig.logAutoRemove)
            offFlags |= DbConstants.DB_LOG_AUTOREMOVE;

        if (logInMemory && !oldConfig.logInMemory)
            onFlags |= DbConstants.DB_LOG_INMEMORY;
        if (!logInMemory && oldConfig.logInMemory)
            offFlags |= DbConstants.DB_LOG_INMEMORY;

        if (noLocking && !oldConfig.noLocking)
            onFlags |= DbConstants.DB_NOLOCKING;
        if (!noLocking && oldConfig.noLocking)
            offFlags |= DbConstants.DB_NOLOCKING;

        if (noMMap && !oldConfig.noMMap)
            onFlags |= DbConstants.DB_NOMMAP;
        if (!noMMap && oldConfig.noMMap)
            offFlags |= DbConstants.DB_NOMMAP;

        if (noPanic && !oldConfig.noPanic)
            onFlags |= DbConstants.DB_NOPANIC;
        if (!noPanic && oldConfig.noPanic)
            offFlags |= DbConstants.DB_NOPANIC;

        if (overwrite && !oldConfig.overwrite)
            onFlags |= DbConstants.DB_OVERWRITE;
        if (!overwrite && oldConfig.overwrite)
            offFlags |= DbConstants.DB_OVERWRITE;

        if (txnNoSync && !oldConfig.txnNoSync)
            onFlags |= DbConstants.DB_TXN_NOSYNC;
        if (!txnNoSync && oldConfig.txnNoSync)
            offFlags |= DbConstants.DB_TXN_NOSYNC;

        if (txnNotDurable && !oldConfig.txnNotDurable)
            onFlags |= DbConstants.DB_TXN_NOT_DURABLE;
        if (!txnNotDurable && oldConfig.txnNotDurable)
            offFlags |= DbConstants.DB_TXN_NOT_DURABLE;

        if (txnWriteNoSync && !oldConfig.txnWriteNoSync)
            onFlags |= DbConstants.DB_TXN_WRITE_NOSYNC;
        if (!txnWriteNoSync && oldConfig.txnWriteNoSync)
            offFlags |= DbConstants.DB_TXN_WRITE_NOSYNC;

        if (yieldCPU && !oldConfig.yieldCPU)
            onFlags |= DbConstants.DB_YIELDCPU;
        if (!yieldCPU && oldConfig.yieldCPU)
            offFlags |= DbConstants.DB_YIELDCPU;

        if (onFlags != 0)
            dbenv.set_flags(onFlags, true);
        if (offFlags != 0)
            dbenv.set_flags(offFlags, false);

        /* Verbose flags */
        onFlags = 0;
        offFlags = 0;

        if (verboseDeadlock && !oldConfig.verboseDeadlock)
            onFlags |= DbConstants.DB_VERB_DEADLOCK;
        if (!verboseDeadlock && oldConfig.verboseDeadlock)
            offFlags |= DbConstants.DB_VERB_DEADLOCK;

        if (verboseRecovery && !oldConfig.verboseRecovery)
            onFlags |= DbConstants.DB_VERB_RECOVERY;
        if (!verboseRecovery && oldConfig.verboseRecovery)
            offFlags |= DbConstants.DB_VERB_RECOVERY;

        if (verboseReplication && !oldConfig.verboseReplication)
            onFlags |= DbConstants.DB_VERB_REPLICATION;
        if (!verboseReplication && oldConfig.verboseReplication)
            offFlags |= DbConstants.DB_VERB_REPLICATION;

        if (verboseWaitsFor && !oldConfig.verboseWaitsFor)
            onFlags |= DbConstants.DB_VERB_WAITSFOR;
        if (!verboseWaitsFor && oldConfig.verboseWaitsFor)
            offFlags |= DbConstants.DB_VERB_WAITSFOR;

        if (onFlags != 0)
            dbenv.set_verbose(onFlags, true);
        if (offFlags != 0)
            dbenv.set_verbose(offFlags, false);

        /* Callbacks */
        if (feedbackHandler != oldConfig.feedbackHandler)
            dbenv.set_feedback(feedbackHandler);
        if (logRecordHandler != oldConfig.logRecordHandler)
            dbenv.set_app_dispatch(logRecordHandler);
        if (messageHandler != oldConfig.messageHandler)
            dbenv.set_msgcall(messageHandler);
        if (panicHandler != oldConfig.panicHandler)
            dbenv.set_paniccall(panicHandler);
        if (replicationTransport != oldConfig.replicationTransport)
            dbenv.set_rep_transport(envid, replicationTransport);

        /* Other settings */
        if (cacheSize != oldConfig.cacheSize ||
            cacheCount != oldConfig.cacheCount)
            dbenv.set_cachesize(cacheSize, cacheCount);
        for (final java.util.Enumeration e = dataDirs.elements();
            e.hasMoreElements();) {
            final String dir = (String)e.nextElement();
            if (!oldConfig.dataDirs.contains(dir))
                dbenv.set_data_dir(dir);
        }
        if (!lockConflictsEqual(lockConflicts, oldConfig.lockConflicts))
            dbenv.set_lk_conflicts(lockConflicts);
        if (lockDetectMode != oldConfig.lockDetectMode)
            dbenv.set_lk_detect(lockDetectMode.getFlag());
        if (maxLocks != oldConfig.maxLocks)
            dbenv.set_lk_max_locks(maxLocks);
        if (maxLockers != oldConfig.maxLockers)
            dbenv.set_lk_max_lockers(maxLockers);
        if (maxLockObjects != oldConfig.maxLockObjects)
            dbenv.set_lk_max_objects(maxLockObjects);
        if (maxLogFileSize != oldConfig.maxLogFileSize)
            dbenv.set_lg_max(maxLogFileSize);
        if (logBufferSize != oldConfig.logBufferSize)
            dbenv.set_lg_bsize(logBufferSize);
        if (logDirectory != oldConfig.logDirectory && logDirectory != null &&
            !logDirectory.equals(oldConfig.logDirectory))
            dbenv.set_lg_dir(logDirectory.toString());
        if (logRegionSize != oldConfig.logRegionSize)
            dbenv.set_lg_regionmax(logRegionSize);
        if (messageStream != oldConfig.messageStream)
            dbenv.set_message_stream(messageStream);
        if (mmapSize != oldConfig.mmapSize)
            dbenv.set_mp_mmapsize(mmapSize);
        if (password != null)
            dbenv.set_encrypt(password, DbConstants.DB_ENCRYPT_AES);
        if (replicationLimit != oldConfig.replicationLimit)
            dbenv.set_rep_limit(replicationLimit);
        if (segmentId != oldConfig.segmentId)
            dbenv.set_shm_key(segmentId);
        if (testAndSetSpins != oldConfig.testAndSetSpins)
            dbenv.set_tas_spins(testAndSetSpins);
        if (lockTimeout != oldConfig.lockTimeout)
            dbenv.set_timeout(lockTimeout, DbConstants.DB_SET_LOCK_TIMEOUT);
        if (txnMaxActive != oldConfig.txnMaxActive)
            dbenv.set_tx_max(txnMaxActive);
        if (txnTimeout != oldConfig.txnTimeout)
            dbenv.set_timeout(txnTimeout, DbConstants.DB_SET_TXN_TIMEOUT);
        if (txnTimestamp != oldConfig.txnTimestamp && txnTimestamp != null &&
            !txnTimestamp.equals(oldConfig.txnTimestamp))
            dbenv.set_tx_timestamp(txnTimestamp);
        if (temporaryDirectory != oldConfig.temporaryDirectory &&
            temporaryDirectory != null &&
            !temporaryDirectory.equals(oldConfig.temporaryDirectory))
            dbenv.set_tmp_dir(temporaryDirectory);
    }

    /* package */
    EnvironmentConfig(final DbEnv dbenv)
        throws DatabaseException {

        final int openFlags = dbenv.get_open_flags();

        allowCreate = ((openFlags & DbConstants.DB_CREATE) != 0);
        initializeCache = ((openFlags & DbConstants.DB_INIT_MPOOL) != 0);
        initializeCDB = ((openFlags & DbConstants.DB_INIT_CDB) != 0);
        initializeLocking = ((openFlags & DbConstants.DB_INIT_LOCK) != 0);
        initializeLogging = ((openFlags & DbConstants.DB_INIT_LOG) != 0);
        initializeReplication = ((openFlags & DbConstants.DB_INIT_REP) != 0);
        joinEnvironment = ((openFlags & DbConstants.DB_JOINENV) != 0);
        lockDown = ((openFlags & DbConstants.DB_LOCKDOWN) != 0);
        isPrivate = ((openFlags & DbConstants.DB_PRIVATE) != 0);
        readOnly = ((openFlags & DbConstants.DB_RDONLY) != 0);
        runRecovery = ((openFlags & DbConstants.DB_RECOVER) != 0);
        runFatalRecovery = ((openFlags & DbConstants.DB_RECOVER_FATAL) != 0);
        systemMemory = ((openFlags & DbConstants.DB_SYSTEM_MEM) != 0);
        threaded = ((openFlags & DbConstants.DB_THREAD) != 0);
        transactional = ((openFlags & DbConstants.DB_INIT_TXN) != 0);
        useEnvironment = ((openFlags & DbConstants.DB_USE_ENVIRON) != 0);
        useEnvironmentRoot =
            ((openFlags & DbConstants.DB_USE_ENVIRON_ROOT) != 0);

        final int envFlags = dbenv.get_flags();

        cdbLockAllDatabases = ((envFlags & DbConstants.DB_CDB_ALLDB) != 0);
        directDatabaseIO = ((envFlags & DbConstants.DB_DIRECT_DB) != 0);
        directLogIO = ((envFlags & DbConstants.DB_DIRECT_LOG) != 0);
        dsyncLog = ((envFlags & DbConstants.DB_DSYNC_LOG) != 0);
        initializeRegions = ((envFlags & DbConstants.DB_REGION_INIT) != 0);
        logAutoRemove = ((envFlags & DbConstants.DB_LOG_AUTOREMOVE) != 0);
        logInMemory = ((envFlags & DbConstants.DB_LOG_INMEMORY) != 0);
        noLocking = ((envFlags & DbConstants.DB_NOLOCKING) != 0);
        noMMap = ((envFlags & DbConstants.DB_NOMMAP) != 0);
        noPanic = ((envFlags & DbConstants.DB_NOPANIC) != 0);
        overwrite = ((envFlags & DbConstants.DB_OVERWRITE) != 0);
        txnNoSync = ((envFlags & DbConstants.DB_TXN_NOSYNC) != 0);
        txnNotDurable = ((envFlags & DbConstants.DB_TXN_NOT_DURABLE) != 0);
        txnWriteNoSync = ((envFlags & DbConstants.DB_TXN_WRITE_NOSYNC) != 0);
        yieldCPU = ((envFlags & DbConstants.DB_YIELDCPU) != 0);

        /* Verbose flags */
        verboseDeadlock = dbenv.get_verbose(DbConstants.DB_VERB_DEADLOCK);
        verboseRecovery = dbenv.get_verbose(DbConstants.DB_VERB_RECOVERY);
        verboseReplication = dbenv.get_verbose(DbConstants.DB_VERB_REPLICATION);
        verboseWaitsFor = dbenv.get_verbose(DbConstants.DB_VERB_WAITSFOR);

        /* Callbacks */
        errorHandler = dbenv.get_errcall();
        feedbackHandler = dbenv.get_feedback();
        logRecordHandler = dbenv.get_app_dispatch();
        messageHandler = dbenv.get_msgcall();
        panicHandler = dbenv.get_paniccall();
        // XXX: replicationTransport and envid aren't available?

        /* Other settings */
        if (initializeCache) {
            cacheSize = dbenv.get_cachesize();
            cacheCount = dbenv.get_cachesize_ncache();
            mmapSize = dbenv.get_mp_mmapsize();
        }

        String[] dataDirArray = dbenv.get_data_dirs();
        if (dataDirArray == null)
            dataDirArray = new String[0];
        dataDirs = new java.util.Vector(dataDirArray.length);
        dataDirs.setSize(dataDirArray.length);
        for (int i = 0; i < dataDirArray.length; i++)
            dataDirs.set(i, dataDirArray[i]);

        errorPrefix = dbenv.get_errpfx();
        errorStream = dbenv.get_error_stream();

        if (initializeLocking) {
            lockConflicts = dbenv.get_lk_conflicts();
            lockDetectMode = LockDetectMode.fromFlag(dbenv.get_lk_detect());
            lockTimeout = dbenv.get_timeout(DbConstants.DB_SET_LOCK_TIMEOUT);
            maxLocks = dbenv.get_lk_max_locks();
            maxLockers = dbenv.get_lk_max_lockers();
            maxLockObjects = dbenv.get_lk_max_objects();
            txnTimeout = dbenv.get_timeout(DbConstants.DB_SET_TXN_TIMEOUT);
        } else {
            lockConflicts = null;
            lockDetectMode = LockDetectMode.NONE;
            lockTimeout = 0L;
            maxLocks = 0;
            maxLockers = 0;
            maxLockObjects = 0;
            txnTimeout = 0L;
        }
        if (initializeLogging) {
            maxLogFileSize = dbenv.get_lg_max();
            logBufferSize = dbenv.get_lg_bsize();
            logDirectory = (dbenv.get_lg_dir() == null) ? null :
                new java.io.File(dbenv.get_lg_dir());
            logRegionSize = dbenv.get_lg_regionmax();
        } else {
            maxLogFileSize = 0;
            logBufferSize = 0;
            logDirectory = null;
            logRegionSize = 0;
        }
        messageStream = dbenv.get_message_stream();

        // XXX: intentional information loss?
        password = (dbenv.get_encrypt_flags() == 0) ? null : "";

        if (initializeReplication) {
            replicationLimit = dbenv.get_rep_limit();
        } else {
            replicationLimit = 0L;
        }

        // XXX: no way to find RPC server?
        rpcServer = null;
        rpcClientTimeout = 0;
        rpcServerTimeout = 0;

        segmentId = dbenv.get_shm_key();
        testAndSetSpins = dbenv.get_tas_spins();
        if (transactional) {
            txnMaxActive = dbenv.get_tx_max();
            final long txnTimestampSeconds = dbenv.get_tx_timestamp();
            if (txnTimestampSeconds != 0L)
                txnTimestamp = new java.util.Date(txnTimestampSeconds * 1000);
            else
                txnTimestamp = null;
        } else {
            txnMaxActive = 0;
            txnTimestamp = null;
        }
        temporaryDirectory = dbenv.get_tmp_dir();
    }
}
