/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 *
 * $Id: EnvironmentConfig.java,v 12.29 2007/07/06 00:22:54 mjc Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.ReplicationHostAddress;

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
    private long cacheMax = 0L;
    private java.util.Vector dataDirs = new java.util.Vector();
    private int envid = 0;
    private String errorPrefix = null;
    private java.io.OutputStream errorStream = null;
    private java.io.OutputStream messageStream = null;
    private byte[][] lockConflicts = null;
    private LockDetectMode lockDetectMode = LockDetectMode.NONE;
    private int maxLocks = 0;
    private int maxLockers = 0;
    private int maxLockObjects = 0;
    private int maxLogFileSize = 0;
    private int logBufferSize = 0;
    private java.io.File logDirectory = null;
    private int logFileMode = 0;
    private int logRegionSize = 0;
    private int maxMutexes = 0;
    private int maxOpenFiles = 0;
    private int maxWrite = 0;
    private long maxWriteSleep = 0L;
    private int mutexAlignment = 0;
    private int mutexIncrement = 0;
    private int mutexTestAndSetSpins = 0;
    private long mmapSize = 0L;
    private String password = null;
    private int replicationLease = 0;
    private long replicationLimit = 0L;
    private int replicationNSites = 0;
    private int replicationPriority = DbConstants.DB_REP_DEFAULT_PRIORITY;
    private int replicationRequestMin = 0;
    private int replicationRequestMax = 0;
    private String rpcServer = null;
    private long rpcClientTimeout = 0L;
    private long rpcServerTimeout = 0L;
    private long segmentId = 0L;
    private long lockTimeout = 0L;
    private int txnMaxActive = 0;
    private long txnTimeout = 0L;
    private java.util.Date txnTimestamp = null;
    private java.io.File temporaryDirectory = null;
    private ReplicationManagerAckPolicy repmgrAckPolicy =
        ReplicationManagerAckPolicy.ALL;
    private ReplicationHostAddress repmgrLocalSiteAddr = null;
    private java.util.Vector repmgrRemoteSites = new java.util.Vector();

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
    private boolean register = false;
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
    private boolean dsyncDatabases = false;
    private boolean dsyncLog = false;
    private boolean initializeRegions = false;
    private boolean logAutoRemove = false;
    private boolean logInMemory = false;
    private boolean multiversion = false;
    private boolean noLocking = false;
    private boolean noMMap = false;
    private boolean noPanic = false;
    private boolean overwrite = false;
    private boolean txnNoSync = false;
    private boolean txnNoWait = false;
    private boolean txnNotDurable = false;
    private boolean txnSnapshot = false;
    private boolean txnWriteNoSync = false;
    private boolean yieldCPU = false;

    /* Verbose Flags */
    private boolean verboseDeadlock = false;
    private boolean verboseFileops = false;
    private boolean verboseFileopsAll = false;
    private boolean verboseRecovery = false;
    private boolean verboseRegister = false;
    private boolean verboseReplication = false;
    private boolean verboseWaitsFor = false;

    /* Callbacks */
    private ErrorHandler errorHandler = null;
    private FeedbackHandler feedbackHandler = null;
    private LogRecordHandler logRecordHandler = null;
    private EventHandler eventHandler = null;
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

    public void setCacheMax(final long cacheMax) {
        this.cacheMax = cacheMax;
    }

    public long getCacheMax() {
        return cacheMax;
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

    public void addDataDir(final java.io.File dataDir) {
        this.dataDirs.add(dataDir);
    }

    /** @deprecated replaced by {@link #addDataDir(java.io.File)}  */
    public void addDataDir(final String dataDir) {
        this.addDataDir(new java.io.File(dataDir));
    }

    public java.io.File[] getDataDirs() {
        final java.io.File[] dirs = new java.io.File[dataDirs.size()];
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

    public void setDsyncDatabases(final boolean dsyncDatabases) {
        this.dsyncDatabases = dsyncDatabases;
    }

    public boolean getDsyncDatabases() {
        return dsyncDatabases;
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

    public void setEventHandler(final EventHandler eventHandler) {
        this.eventHandler = eventHandler;
    }

    public EventHandler getEventHandler() {
        return eventHandler;
    }

    public void setReplicationManagerAckPolicy(
        final ReplicationManagerAckPolicy repmgrAckPolicy)
    {
        this.repmgrAckPolicy = repmgrAckPolicy;
    }

    public ReplicationManagerAckPolicy getReplicationManagerAckPolicy()
    {
        return repmgrAckPolicy;
    }

    public void setReplicationManagerLocalSite(
        final ReplicationHostAddress repmgrLocalSiteAddr)
    {
        this.repmgrLocalSiteAddr = repmgrLocalSiteAddr;
    }

    public ReplicationHostAddress getReplicationManagerLocalSite()
    {
        return repmgrLocalSiteAddr;
    }

    public void replicationManagerAddRemoteSite(
        final ReplicationHostAddress repmgrRemoteAddr)
    {
        this.repmgrRemoteSites.add(repmgrRemoteAddr);
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

    public void setLogFileMode(final int logFileMode) {
        this.logFileMode = logFileMode;
    }

    public int getLogFileMode() {
        return logFileMode;
    }

    public void setLogRegionSize(final int logRegionSize) {
        this.logRegionSize = logRegionSize;
    }

    public int getLogRegionSize() {
        return logRegionSize;
    }

    public void setMaxOpenFiles(final int maxOpenFiles) {
        this.maxOpenFiles = maxOpenFiles;
    }

    public int getMaxOpenFiles() {
        return maxOpenFiles;
    }

    public void setMaxWrite(final int maxWrite, final long maxWriteSleep) {
        this.maxWrite = maxWrite;
        this.maxWriteSleep = maxWriteSleep;
    }

    public int getMaxWrite() {
        return maxWrite;
    }

    public long getMaxWriteSleep() {
        return maxWriteSleep;
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

    public void setMultiversion(final boolean multiversion) {
        this.multiversion = multiversion;
    }

    public boolean getMultiversion() {
        return multiversion;
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

    public void setReplicationLease(final int replicationLease) {
        this.replicationLease = replicationLease;
    }

    public int getReplicationLease() {
        return replicationLease;
    }

    public void setReplicationLimit(final long replicationLimit) {
        this.replicationLimit = replicationLimit;
    }

    public long getReplicationLimit() {
        return replicationLimit;
    }

    public void setReplicationRequestMin(final int replicationRequestMin) {
        this.replicationRequestMin = replicationRequestMin;
    }

    public int getReplicationRequestMin() {
        return replicationRequestMin;
    }

    public void setReplicationRequestMax(final int replicationRequestMax) {
        this.replicationRequestMax = replicationRequestMax;
    }

    public int getReplicationRequestMax() {
        return replicationRequestMax;
    }

    public void setReplicationTransport(final int envid,
        final ReplicationTransport replicationTransport) {

        this.envid = envid;
        this.replicationTransport = replicationTransport;
    }

    public ReplicationTransport getReplicationTransport() {
        return replicationTransport;
    }

    public void setRegister(final boolean register) {
        this.register = register;
    }

    public boolean getRegister() {
        return register;
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

    public void setTemporaryDirectory(final java.io.File temporaryDirectory) {
        this.temporaryDirectory = temporaryDirectory;
    }

    /** @deprecated replaced by {@link #setTemporaryDirectory(java.io.File)} */
    public void setTemporaryDirectory(final String temporaryDirectory) {
        this.setTemporaryDirectory(new java.io.File(temporaryDirectory));
    }

    public java.io.File getTemporaryDirectory() {
        return temporaryDirectory;
    }

    public void setMutexAlignment(final int mutexAlignment) {
        this.mutexAlignment = mutexAlignment;
    }

    public int getMutexAlignment() {
        return mutexAlignment;
    }

    public void setMutexIncrement(final int mutexIncrement) {
        this.mutexIncrement = mutexIncrement;
    }

    public int getMutexIncrement() {
        return mutexIncrement;
    }

    public void setMaxMutexes(final int maxMutexes) {
        this.maxMutexes = maxMutexes;
    }

    public int getMaxMutexes() {
        return maxMutexes;
    }

    public void setMutexTestAndSetSpins(final int mutexTestAndSetSpins) {
        this.mutexTestAndSetSpins = mutexTestAndSetSpins;
    }

    public int getMutexTestAndSetSpins() {
        return mutexTestAndSetSpins;
    }

    public void setReplicationNumSites(final int replicationNSites) {
        this.replicationNSites = replicationNSites;
    }

    public int getReplicationNumSites() {
        return replicationNSites;
    }

    public void setReplicationPriority(final int replicationPriority) {
        this.replicationPriority = replicationPriority;
    }

    public int getReplicationPriority() {
        return replicationPriority;
    }

    /** @deprecated renamed {@link #setMutexTestAndSetSpins} */
    public void setTestAndSetSpins(final int mutexTestAndSetSpins) {
        setMutexTestAndSetSpins(mutexTestAndSetSpins);
    }

    /** @deprecated renamed {@link #getMutexTestAndSetSpins} */
    public int getTestAndSetSpins() {
        return getMutexTestAndSetSpins();
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

    public void setTxnNoWait(final boolean txnNoWait) {
        this.txnNoWait = txnNoWait;
    }

    public boolean getTxnNoWait() {
        return txnNoWait;
    }

    public void setTxnNotDurable(final boolean txnNotDurable) {
        this.txnNotDurable = txnNotDurable;
    }

    public boolean getTxnNotDurable() {
        return txnNotDurable;
    }

    public void setTxnSnapshot(final boolean txnSnapshot) {
        this.txnSnapshot = txnSnapshot;
    }

    public boolean getTxnSnapshot() {
        return txnSnapshot;
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

    public void setVerbose(final VerboseConfig flag, boolean enable) {
        int iflag = flag.getInternalFlag();
        switch (iflag) {
        case DbConstants.DB_VERB_DEADLOCK:
            verboseDeadlock = enable;
            break;
        case DbConstants.DB_VERB_FILEOPS:
            verboseFileops = enable;
            break;
        case DbConstants.DB_VERB_FILEOPS_ALL:
            verboseFileopsAll = enable;
            break;
        case DbConstants.DB_VERB_RECOVERY:
            verboseRecovery = enable;
            break;
        case DbConstants.DB_VERB_REGISTER:
            verboseRegister = enable;
            break;
        case DbConstants.DB_VERB_REPLICATION:
            verboseReplication = enable;
            break;
        case DbConstants.DB_VERB_WAITSFOR:
            verboseWaitsFor = enable;
            break;
        default:
            throw new IllegalArgumentException(
                "Unknown verbose flag: " + DbEnv.strerror(iflag));
        }
    }

    public boolean getVerbose(final VerboseConfig flag) {
        int iflag = flag.getInternalFlag();
        switch (iflag) {
        case DbConstants.DB_VERB_DEADLOCK:
            return verboseDeadlock;
        case DbConstants.DB_VERB_FILEOPS:
            return verboseFileops;
        case DbConstants.DB_VERB_FILEOPS_ALL:
            return verboseFileopsAll;
        case DbConstants.DB_VERB_RECOVERY:
            return verboseRecovery;
        case DbConstants.DB_VERB_REGISTER:
            return verboseRegister;
        case DbConstants.DB_VERB_REPLICATION:
            return verboseReplication;
        case DbConstants.DB_VERB_WAITSFOR:
            return verboseWaitsFor;
        default:
            throw new IllegalArgumentException(
                "Unknown verbose flag: " + DbEnv.strerror(iflag));
       }
    }

    /** @deprecated */
    public void setVerboseDeadlock(final boolean verboseDeadlock) {
        this.verboseDeadlock = verboseDeadlock;
    }

    /** @deprecated */
    public boolean getVerboseDeadlock() {
        return verboseDeadlock;
    }

    /** @deprecated */
    public void setVerboseRecovery(final boolean verboseRecovery) {
        this.verboseRecovery = verboseRecovery;
    }

    /** @deprecated */
    public boolean getVerboseRecovery() {
        return verboseRecovery;
    }

    /** @deprecated */
    public void setVerboseRegister(final boolean verboseRegister) {
        this.verboseRegister = verboseRegister;
    }

    /** @deprecated */
    public boolean getVerboseRegister() {
        return verboseRegister;
    }

    /** @deprecated */
    public void setVerboseReplication(final boolean verboseReplication) {
        this.verboseReplication = verboseReplication;
    }

    /** @deprecated */
    public boolean getVerboseReplication() {
        return verboseReplication;
    }

    /** @deprecated */
    public void setVerboseWaitsFor(final boolean verboseWaitsFor) {
        this.verboseWaitsFor = verboseWaitsFor;
    }

    /** @deprecated */
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
        openFlags |= register ? DbConstants.DB_REGISTER : 0;
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
            dbenv.set_rpc_server(rpcServer,
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

        if (dsyncDatabases && !oldConfig.dsyncDatabases)
            onFlags |= DbConstants.DB_DSYNC_DB;
        if (!dsyncDatabases && oldConfig.dsyncDatabases)
            offFlags |= DbConstants.DB_DSYNC_DB;

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

        if (multiversion && !oldConfig.multiversion)
            onFlags |= DbConstants.DB_MULTIVERSION;
        if (!multiversion && oldConfig.multiversion)
            offFlags |= DbConstants.DB_MULTIVERSION;

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

        if (txnNoWait && !oldConfig.txnNoWait)
            onFlags |= DbConstants.DB_TXN_NOWAIT;
        if (!txnNoWait && oldConfig.txnNoWait)
            offFlags |= DbConstants.DB_TXN_NOWAIT;

        if (txnNotDurable && !oldConfig.txnNotDurable)
            onFlags |= DbConstants.DB_TXN_NOT_DURABLE;
        if (!txnNotDurable && oldConfig.txnNotDurable)
            offFlags |= DbConstants.DB_TXN_NOT_DURABLE;

        if (txnSnapshot && !oldConfig.txnSnapshot)
            onFlags |= DbConstants.DB_TXN_SNAPSHOT;
        if (!txnSnapshot && oldConfig.txnSnapshot)
            offFlags |= DbConstants.DB_TXN_SNAPSHOT;

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
        if (verboseDeadlock && !oldConfig.verboseDeadlock)
            dbenv.set_verbose(DbConstants.DB_VERB_DEADLOCK, true);
        if (!verboseDeadlock && oldConfig.verboseDeadlock)
            dbenv.set_verbose(DbConstants.DB_VERB_DEADLOCK, false);

        if (verboseFileops && !oldConfig.verboseFileops)
            dbenv.set_verbose(DbConstants.DB_VERB_FILEOPS, true);
        if (!verboseFileops && oldConfig.verboseFileops)
            dbenv.set_verbose(DbConstants.DB_VERB_FILEOPS, false);

        if (verboseFileopsAll && !oldConfig.verboseFileopsAll)
            dbenv.set_verbose(DbConstants.DB_VERB_FILEOPS_ALL, true);
        if (!verboseFileopsAll && oldConfig.verboseFileopsAll)
            dbenv.set_verbose(DbConstants.DB_VERB_FILEOPS_ALL, false);

        if (verboseRecovery && !oldConfig.verboseRecovery)
            dbenv.set_verbose(DbConstants.DB_VERB_RECOVERY, true);
        if (!verboseRecovery && oldConfig.verboseRecovery)
            dbenv.set_verbose(DbConstants.DB_VERB_RECOVERY, false);

        if (verboseRegister && !oldConfig.verboseRegister)
            dbenv.set_verbose(DbConstants.DB_VERB_REGISTER, true);
        if (!verboseRegister && oldConfig.verboseRegister)
            dbenv.set_verbose(DbConstants.DB_VERB_REGISTER, false);

        if (verboseReplication && !oldConfig.verboseReplication)
            dbenv.set_verbose(DbConstants.DB_VERB_REPLICATION, true);
        if (!verboseReplication && oldConfig.verboseReplication)
            dbenv.set_verbose(DbConstants.DB_VERB_REPLICATION, false);

        if (verboseWaitsFor && !oldConfig.verboseWaitsFor)
            dbenv.set_verbose(DbConstants.DB_VERB_WAITSFOR, true);
        if (!verboseWaitsFor && oldConfig.verboseWaitsFor)
            dbenv.set_verbose(DbConstants.DB_VERB_WAITSFOR, false);

        /* Callbacks */
        if (feedbackHandler != oldConfig.feedbackHandler)
            dbenv.set_feedback(feedbackHandler);
        if (logRecordHandler != oldConfig.logRecordHandler)
            dbenv.set_app_dispatch(logRecordHandler);
        if (eventHandler != oldConfig.eventHandler)
            dbenv.set_event_notify(eventHandler);
        if (messageHandler != oldConfig.messageHandler)
            dbenv.set_msgcall(messageHandler);
        if (panicHandler != oldConfig.panicHandler)
            dbenv.set_paniccall(panicHandler);
        if (replicationTransport != oldConfig.replicationTransport)
            dbenv.rep_set_transport(envid, replicationTransport);

        /* Other settings */
        if (cacheSize != oldConfig.cacheSize ||
            cacheCount != oldConfig.cacheCount)
            dbenv.set_cachesize(cacheSize, cacheCount);
        if (cacheMax != oldConfig.cacheMax)
            dbenv.set_cache_max(cacheMax);
        for (final java.util.Enumeration e = dataDirs.elements();
            e.hasMoreElements();) {
            final java.io.File dir = (java.io.File)e.nextElement();
            if (!oldConfig.dataDirs.contains(dir))
                dbenv.set_data_dir(dir.toString());
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
        if (logFileMode != oldConfig.logFileMode)
            dbenv.set_lg_filemode(logFileMode);
        if (logRegionSize != oldConfig.logRegionSize)
            dbenv.set_lg_regionmax(logRegionSize);
        if (maxOpenFiles != oldConfig.maxOpenFiles)
            dbenv.set_mp_max_openfd(maxOpenFiles);
        if (maxWrite != oldConfig.maxWrite ||
            maxWriteSleep != oldConfig.maxWriteSleep)
            dbenv.set_mp_max_write(maxWrite, maxWriteSleep);
        if (messageStream != oldConfig.messageStream)
            dbenv.set_message_stream(messageStream);
        if (mmapSize != oldConfig.mmapSize)
            dbenv.set_mp_mmapsize(mmapSize);
        if (password != null)
            dbenv.set_encrypt(password, DbConstants.DB_ENCRYPT_AES);
        if (replicationLease != oldConfig.replicationLease)
            dbenv.rep_set_lease(replicationLease, 0);
        if (replicationLimit != oldConfig.replicationLimit)
            dbenv.rep_set_limit(replicationLimit);
        if (replicationRequestMin != oldConfig.replicationRequestMin ||
            replicationRequestMax != oldConfig.replicationRequestMax)
            dbenv.set_rep_request(replicationRequestMin, replicationRequestMax);
        if (segmentId != oldConfig.segmentId)
            dbenv.set_shm_key(segmentId);
        if (mutexAlignment != oldConfig.mutexAlignment)
            dbenv.mutex_set_align(mutexAlignment);
        if (mutexIncrement != oldConfig.mutexIncrement)
            dbenv.mutex_set_increment(mutexIncrement);
        if (maxMutexes != oldConfig.maxMutexes)
            dbenv.mutex_set_max(maxMutexes);
        if (mutexTestAndSetSpins != oldConfig.mutexTestAndSetSpins)
            dbenv.mutex_set_tas_spins(mutexTestAndSetSpins);
        if (replicationNSites != oldConfig.replicationNSites)
            dbenv.rep_set_nsites(replicationNSites);
        if (replicationPriority != oldConfig.replicationPriority)
            dbenv.rep_set_priority(replicationPriority);
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
            dbenv.set_tmp_dir(temporaryDirectory.toString());
        if (repmgrAckPolicy != oldConfig.repmgrAckPolicy)
            dbenv.repmgr_set_ack_policy(repmgrAckPolicy.getId());
        if (repmgrLocalSiteAddr != oldConfig.repmgrLocalSiteAddr) {
            dbenv.repmgr_set_local_site(
                repmgrLocalSiteAddr.host, repmgrLocalSiteAddr.port, 0);
        }
        for (java.util.Enumeration elems = repmgrRemoteSites.elements();
            elems.hasMoreElements();)
        {
            ReplicationHostAddress nextAddr =
                (ReplicationHostAddress)elems.nextElement();
            dbenv.repmgr_add_remote_site(nextAddr.host, nextAddr.port,
                nextAddr.isPeer ? DbConstants.DB_REPMGR_PEER : 0);
        }
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
        register = ((openFlags & DbConstants.DB_REGISTER) != 0);
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
        dsyncDatabases = ((envFlags & DbConstants.DB_DSYNC_DB) != 0);
        dsyncLog = ((envFlags & DbConstants.DB_DSYNC_LOG) != 0);
        initializeRegions = ((envFlags & DbConstants.DB_REGION_INIT) != 0);
        logAutoRemove = ((envFlags & DbConstants.DB_LOG_AUTOREMOVE) != 0);
        logInMemory = ((envFlags & DbConstants.DB_LOG_INMEMORY) != 0);
        multiversion = ((envFlags & DbConstants.DB_MULTIVERSION) != 0);
        noLocking = ((envFlags & DbConstants.DB_NOLOCKING) != 0);
        noMMap = ((envFlags & DbConstants.DB_NOMMAP) != 0);
        noPanic = ((envFlags & DbConstants.DB_NOPANIC) != 0);
        overwrite = ((envFlags & DbConstants.DB_OVERWRITE) != 0);
        txnNoSync = ((envFlags & DbConstants.DB_TXN_NOSYNC) != 0);
        txnNoWait = ((envFlags & DbConstants.DB_TXN_NOWAIT) != 0);
        txnNotDurable = ((envFlags & DbConstants.DB_TXN_NOT_DURABLE) != 0);
        txnSnapshot = ((envFlags & DbConstants.DB_TXN_SNAPSHOT) != 0);
        txnWriteNoSync = ((envFlags & DbConstants.DB_TXN_WRITE_NOSYNC) != 0);
        yieldCPU = ((envFlags & DbConstants.DB_YIELDCPU) != 0);

        /* Verbose flags */
        verboseDeadlock = dbenv.get_verbose(DbConstants.DB_VERB_DEADLOCK);
        verboseFileops = dbenv.get_verbose(DbConstants.DB_VERB_FILEOPS);
        verboseFileopsAll = dbenv.get_verbose(DbConstants.DB_VERB_FILEOPS_ALL);
        verboseRecovery = dbenv.get_verbose(DbConstants.DB_VERB_RECOVERY);
        verboseRegister = dbenv.get_verbose(DbConstants.DB_VERB_REGISTER);
        verboseReplication = dbenv.get_verbose(DbConstants.DB_VERB_REPLICATION);
        verboseWaitsFor = dbenv.get_verbose(DbConstants.DB_VERB_WAITSFOR);

        /* Callbacks */
        errorHandler = dbenv.get_errcall();
        feedbackHandler = dbenv.get_feedback();
        logRecordHandler = dbenv.get_app_dispatch();
        eventHandler = dbenv.get_event_notify();
        messageHandler = dbenv.get_msgcall();
        panicHandler = dbenv.get_paniccall();
        // XXX: replicationTransport and envid aren't available?

        /* Other settings */
        if (initializeCache) {
            cacheSize = dbenv.get_cachesize();
            cacheMax = dbenv.get_cache_max();
            cacheCount = dbenv.get_cachesize_ncache();
            mmapSize = dbenv.get_mp_mmapsize();
            maxOpenFiles = dbenv.get_mp_max_openfd();
            maxWrite = dbenv.get_mp_max_write();
            maxWriteSleep = dbenv.get_mp_max_write_sleep();
        }

        String[] dataDirArray = dbenv.get_data_dirs();
        if (dataDirArray == null)
            dataDirArray = new String[0];
        dataDirs = new java.util.Vector(dataDirArray.length);
        dataDirs.setSize(dataDirArray.length);
        for (int i = 0; i < dataDirArray.length; i++)
            dataDirs.set(i, new java.io.File(dataDirArray[i]));

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
            logFileMode = dbenv.get_lg_filemode();
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
            replicationLimit = dbenv.rep_get_limit();
            // XXX: no way to find out replicationRequest{Min,Max}
            repmgrRemoteSites = new java.util.Vector(
                java.util.Arrays.asList(dbenv.repmgr_site_list()));
        } else {
            replicationLimit = 0L;
            replicationRequestMin = 0;
            replicationRequestMax = 0;
        }

        // XXX: no way to find RPC server?
        rpcServer = null;
        rpcClientTimeout = 0;
        rpcServerTimeout = 0;

        segmentId = dbenv.get_shm_key();
        mutexAlignment = dbenv.mutex_get_align();
        mutexIncrement = dbenv.mutex_get_increment();
        maxMutexes = dbenv.mutex_get_max();
        mutexTestAndSetSpins = dbenv.mutex_get_tas_spins();
        replicationNSites = dbenv.rep_get_nsites();
        replicationPriority = dbenv.rep_get_priority();
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
        temporaryDirectory = new java.io.File(dbenv.get_tmp_dir());
    }
}
